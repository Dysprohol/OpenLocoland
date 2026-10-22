// Native implementation of the script value helpers.
//
// A script value is 12 bytes: a flags byte (1 = text, 2 = int), three bytes of padding,
// the int value at +4 and a pointer to the text at +8. ScriptValue_BinaryOp applies the
// arithmetic and comparison opcodes of the script machine (0x06-0x17) to two values;
// the result replaces the left one. ScriptValue_ToString writes the decimal text of an
// int value (what `itoa` gives the scripts), String_ToInt reads text as an int.
//
// The opcodes and their exact behaviour come from the machine code at 0x43da60 (see
// re/symbols.csv and BUGS.md). Callers ignore the return value, which is why this
// returns the stored value instead of the original's leftover EAX.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sv {

struct Value {
    uint8_t flags;        // +0: 1 = text, 2 = int
    uint8_t reserved[3];
    int32_t value;        // +4
    char* text;           // +8
};

// A text as int, the way String_ToInt (0x43d8c0) does it: a string whose second
// character is 'x' goes through sscanf "%i", everything else through atoi. `converted`
// is false when "%i" converts nothing - the original then returns the output slot of
// sscanf, which holds a pointer (BUGS.md F9).
inline int32_t ParseText(const char* text, bool* converted = nullptr) {
    if (text[0] != '\0' && text[1] == 'x') {
        int value = 0;
        const int count = std::sscanf(text, "%i", &value);
        if (converted) *converted = count == 1;
        return value;
    }
    if (converted) *converted = true;
    return std::atoi(text);
}

inline int32_t ToInt(const Value& value) {
    return (value.flags & 1) ? ParseText(value.text) : value.value;
}

// The value as a number, with the same text rule; false when a text does not convert.
inline bool ToNumber(const Value& value, int32_t* number) {
    if (!(value.flags & 1)) {
        *number = value.value;
        return true;
    }
    bool converted = false;
    *number = ParseText(value.text, &converted);
    return converted;
}

// The game's String_Assign (0x4264c0): the String at `text_field` (a char* field) gets
// a copy of the String at `source_field`. Like String_Append it allocates with the
// game's CRT, so the text stays owned by the game.
using AssignFn = void* (__cdecl*)(void* text_field, void* source_field);

// The game's String_Assign as an AssignFn (defined in script_value.cpp).
void* __cdecl AssignText(void* text_field, void* source_field);

// Value as text: an int is written in decimal into `text` (itoa + String_Assign in the
// original, which encodes the same for every int), a text stays as it is. Returns the
// text field, like the original's char**.
inline char** ToString(Value* value, AssignFn assign) {
    if (!(value->flags & 2)) return &value->text;
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%d", value->value);
    char* source = buffer;
    assign(&value->text, &source);
    return &value->text;
}

// The game functions the binary op needs: String_Append (0x4265b0) and Log_Write
// (0x426080). Append allocates with the game's CRT, so strings stay owned by the game.
// The install function wraps the __thiscall method in a plain callback.
using AppendFn = void* (__cdecl*)(void* text, void* other_text);
using LogFn = void (__cdecl*)(void* log, const char* format, ...);

// Applies op (0x06-0x17) to left and right and stores the result in left.
inline int32_t BinaryOp(Value* left, int op, Value* right, AppendFn append, LogFn log,
                        void* game_log) {
    const int32_t r = ToInt(*right);
    // A text on the left is converted before the operation, like the original (it also
    // keeps the value for the text comparisons below, which look at the flags).
    if (left->flags & 1) left->value = ParseText(left->text);
    const int32_t l = left->value;

    switch (op) {
    case 0x06:  // /
        left->flags = 2;
        if (r == 0)
            left->value = 0x0FFFFFFF;                  // the original's "division by zero"
        else if (l == INT32_MIN && r == -1)
            left->value = INT32_MIN;                   // idiv would fault otherwise
        else
            left->value = l / r;
        break;
    case 0x07:  // %  (the original divides without checking r - BUGS.md F8)
        left->flags = 2;
        if (r == 0)
            left->value = 0x0FFFFFFF;                  // same result as division by zero
        else if (r == -1)
            left->value = 0;                           // also covers INT32_MIN % -1
        else
            left->value = l % r;
        break;
    case 0x08:  // +
        if ((right->flags & 1) && (left->flags & 1)) {
            append(&left->text, &right->text);
            left->flags = 1;
        } else {
            left->flags = 2;
            left->value = static_cast<int32_t>(static_cast<uint32_t>(l) +
                                               static_cast<uint32_t>(r));
        }
        break;
    case 0x09:  // -
        left->flags = 2;
        left->value = static_cast<int32_t>(static_cast<uint32_t>(l) -
                                           static_cast<uint32_t>(r));
        break;
    case 0x0A:  // ^
        left->flags = 2;
        left->value = l ^ r;
        break;
    case 0x0B:  // |
        left->flags = 2;
        left->value = l | r;
        break;
    case 0x0C:  // &
        left->flags = 2;
        left->value = l & r;
        break;
    case 0x0D: {  // ==  (two texts compare by content)
        const bool texts = (right->flags & 1) && (left->flags & 1);
        left->value = texts ? (std::strcmp(left->text, right->text) == 0) : (l == r);
        left->flags = 2;
        break;
    }
    case 0x0E:  // ||
        left->flags = 2;
        left->value = (l != 0 || r != 0);
        break;
    case 0x0F:  // >
        left->flags = 2;
        left->value = (l > r);
        break;
    case 0x10:  // <
        left->flags = 2;
        left->value = (l < r);
        break;
    case 0x11:  // >=
        left->flags = 2;
        left->value = (l >= r);
        break;
    case 0x12:  // <=
        left->flags = 2;
        left->value = (l <= r);
        break;
    case 0x13:  // *
        left->flags = 2;
        left->value = static_cast<int32_t>(static_cast<uint32_t>(l) *
                                           static_cast<uint32_t>(r));
        break;
    case 0x14: {  // !=
        const bool texts = (right->flags & 1) && (left->flags & 1);
        left->value = texts ? (std::strcmp(left->text, right->text) != 0) : (l != r);
        left->flags = 2;
        break;
    }
    case 0x15:  // &&
        left->flags = 2;
        left->value = (l != 0 && r != 0);
        break;
    case 0x16:  // >> (arithmetic; the shift count is masked like x86 does)
        left->flags = 2;
        left->value = l >> (r & 31);
        break;
    case 0x17:  // <<
        left->flags = 2;
        left->value = static_cast<int32_t>(static_cast<uint32_t>(l) << (r & 31));
        break;
    default:
        log(game_log, "!!!ERROE!!!LOGIC::Unknown Binary command %i", op);
        left->flags = 2;
        break;
    }
    return left->value;
}

}  // namespace sv
