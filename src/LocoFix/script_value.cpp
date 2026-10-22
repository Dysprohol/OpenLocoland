// Native rewrites of the script value helpers.
//
// String_ToInt (0x43d8c0), ScriptValue_BinaryOp (0x43da60) and ScriptValue_ToString
// (0x4424c0) are small, fully understood functions, so they are reimplemented here
// instead of patched. All three are hooked at their entry with a jump; the C++ versions
// keep the original calling conventions (__thiscall, `ret 8` for the binary op) and the
// original results.
//
// The binary op fixes one crash of the original: `a % 0` executes idiv without a zero
// check (BUGS.md F8). For the same result as division it yields 0x0FFFFFFF now.
#include "script_value.h"
#include "patch.h"

namespace {

constexpr uintptr_t STRING_TO_INT = 0x43d8c0;  // int __thiscall String_ToInt(char** text)
constexpr uintptr_t BINARY_OP     = 0x43da60;  // int __thiscall (ScriptValue*, int op, ScriptValue*)
constexpr uintptr_t TO_STRING     = 0x4424c0;  // char** __thiscall ScriptValue_ToString(ScriptValue*)
constexpr uintptr_t STRING_APPEND = 0x4265b0;  // String_Append, used by text concatenation
constexpr uintptr_t STRING_ASSIGN = 0x4264c0;  // String_Assign, used by the int-to-text conversion
constexpr uintptr_t LOG_WRITE     = 0x426080;  // Log_Write, used by the unknown opcode
constexpr uintptr_t G_LOG         = 0x4a9d5c;  // Log *g_log

// String_Append and String_Assign are __thiscall methods; the rewrites call them through
// these plain callbacks.
using GameAppendFn = void* (__thiscall*)(void* text, void* other_text);
using GameAssignFn = void* (__thiscall*)(void* text, void* source);

void* __cdecl AppendText(void* text, void* other_text) {
    return reinterpret_cast<GameAppendFn>(STRING_APPEND)(text, other_text);
}

int __fastcall StringToInt(void* ecx_text, void* /*edx*/) {
    return sv::ParseText(*static_cast<char**>(ecx_text));
}

// __fastcall puts the game's ECX (the left value) in the first parameter and EDX in the
// second; the two stack arguments keep the __thiscall layout and the callee cleans them
// (ret 8), like the original.
int __fastcall BinaryOp(void* ecx_left, void* /*edx*/, int op, sv::Value* right) {
    return sv::BinaryOp(static_cast<sv::Value*>(ecx_left), op, right, &AppendText,
                        reinterpret_cast<sv::LogFn>(LOG_WRITE),
                        *reinterpret_cast<void**>(G_LOG));
}

// __thiscall with no stack arguments: ECX is the value, the original returns with ret.
char** __fastcall ToString(void* ecx_value, void* /*edx*/) {
    return sv::ToString(static_cast<sv::Value*>(ecx_value), &sv::AssignText);
}

}  // namespace

namespace sv {

void* __cdecl AssignText(void* text_field, void* source_field) {
    return reinterpret_cast<GameAssignFn>(STRING_ASSIGN)(text_field, source_field);
}

}  // namespace sv

void install_script_value() {
    // push ecx / mov eax,[ecx] / cmp byte [eax+1],'x'
    const uint8_t string_to_int_bytes[] = {0x51, 0x8B, 0x01, 0x80, 0x78, 0x01, 0x78};
    if (patch::redirect(STRING_TO_INT, string_to_int_bytes, sizeof string_to_int_bytes,
                        reinterpret_cast<void*>(&StringToInt), "String_ToInt"))
        patch::log("String_ToInt rewritten @ %08X", static_cast<unsigned>(STRING_TO_INT));

    // push ebx / push ebp / mov ebp,[esp+0x10]
    const uint8_t binary_op_bytes[] = {0x53, 0x55, 0x8B, 0x6C, 0x24, 0x10};
    if (patch::redirect(BINARY_OP, binary_op_bytes, sizeof binary_op_bytes,
                        reinterpret_cast<void*>(&BinaryOp), "ScriptValue_BinaryOp"))
        patch::log("ScriptValue_BinaryOp rewritten @ %08X (modulo by zero returns 0x0FFFFFFF)",
                   static_cast<unsigned>(BINARY_OP));

    // push -1 / push 0x4742b7 / mov eax,fs:[0]  (the SEH prologue; it is 13 bytes, the
    // rest of the frame setup is replaced too)
    const uint8_t to_string_bytes[] = {0x6A, 0xFF, 0x68, 0xB7, 0x42, 0x47, 0x00,
                                       0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
    if (patch::redirect(TO_STRING, to_string_bytes, sizeof to_string_bytes,
                        reinterpret_cast<void*>(&ToString), "ScriptValue_ToString"))
        patch::log("ScriptValue_ToString rewritten @ %08X", static_cast<unsigned>(TO_STRING));
}
