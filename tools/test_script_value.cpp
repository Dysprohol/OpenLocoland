// Test host for the native script value helpers (32-bit).
//
// tools\test_script_value.py builds this together with the Unicorn emulation of the
// original functions and compares the results. Each input line is either
//
//     S <text-hex>
//     T <flags> <value> <text-hex>
//     P <flags> <value> <text-hex>       (World_PopInt)
//     Q <flags> <value> <text-hex>       (World_PopString)
//     B <op> <left-flags> <left-value> <left-text-hex> <right-flags> <right-value> <right-text-hex>
//
// where "-" is an empty text. Each output line is "<flags> <value> <text-hex>".
#define _CRT_SECURE_NO_WARNINGS
#include "../src/LocoFix/script_value.h"
#include "../src/LocoFix/world_script.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

char* dup_text(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    std::memcpy(copy, text.c_str(), text.size() + 1);
    return copy;
}

std::string unhex(const char* text) {
    if (text[0] == '-') return std::string();
    std::string result;
    const size_t n = std::strlen(text);
    for (size_t i = 0; i + 1 < n; i += 2) {
        char byte[3] = {text[i], text[i + 1], 0};
        result.push_back(static_cast<char>(std::strtoul(byte, nullptr, 16)));
    }
    return result;
}

std::string to_hex(const char* text) {
    if (!text || !*text) return "-";
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        result.push_back(digits[*p >> 4]);
        result.push_back(digits[*p & 15]);
    }
    return result;
}

// The game's String_Append, replaced for the test.
void* append_text(void* text_field, void* other_field) {
    auto* self = static_cast<char**>(text_field);
    auto* other = static_cast<char**>(other_field);
    const std::string joined = std::string(*self) + *other;
    *self = dup_text(joined);
    return text_field;
}

void __cdecl log_noop(void*, const char*, ...) {}

// The game's String_Assign, replaced for the test (the old text is left allocated; the
// process ends after the cases).
void* assign_text(void* text_field, void* source_field) {
    auto* self = static_cast<char**>(text_field);
    auto* source = static_cast<char**>(source_field);
    *self = dup_text(*source);
    return text_field;
}

// A World with a script stack of one value.
uint8_t g_world[0x400];
sv::Value g_stack[4];

// The VIDs of the test World and the empty one (the game uses g_emptyVid for a number
// that is not loaded); the same numbers are used by the emulation.
void* g_vids[4] = {reinterpret_cast<void*>(0x1000), reinterpret_cast<void*>(0x1001),
                   reinterpret_cast<void*>(0x1002), nullptr};
void* const g_empty_vid = reinterpret_cast<void*>(0x2000);

// The game's log, replaced for the test: the calls are counted.
int g_log_errors = 0;
int g_log_writes = 0;

void log_error(void*, const char*, int, const char*, int) { g_log_errors++; }
void log_write(void*, const char*, ...) { g_log_writes++; }

world::WorldLog g_log{nullptr, log_error, log_write, "LOGIC", "this variable is not unit",
                       "Invalid nvid %s %i"};

// Puts a value on the script stack of the test World.
void setup_world(int flags, int value, const std::string& text) {
    std::memset(g_world, 0, sizeof g_world);
    std::memset(g_stack, 0, sizeof g_stack);
    g_stack[0].flags = static_cast<uint8_t>(flags);
    g_stack[0].value = value;
    g_stack[0].text = dup_text(text);
    *world::script_count(g_world) = 1;
    *reinterpret_cast<sv::Value**>(g_world + world::SCRIPT_VALUES) = g_stack;
    *reinterpret_cast<int32_t*>(g_world + world::VID_COUNT) = 4;
    auto** vids = reinterpret_cast<void**>(g_world + world::VIDS);
    for (int i = 0; i < 4; ++i) vids[i] = g_vids[i];
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    FILE* in = std::fopen(argv[1], "r");
    if (!in) return 2;
    char kind[8];
    while (std::fscanf(in, "%7s", kind) == 1) {
        if (kind[0] == 'S') {
            char text[4096];
            if (std::fscanf(in, "%4095s", text) != 1) break;
            std::printf("%d -\n", sv::ParseText(unhex(text).c_str()));
        } else if (kind[0] == 'T') {
            int flags = 0, value = 0;
            char text[4096];
            if (std::fscanf(in, "%d %d %4095s", &flags, &value, text) != 3) break;
            sv::Value v{};
            v.flags = static_cast<uint8_t>(flags);
            v.value = value;
            v.text = dup_text(unhex(text));
            sv::ToString(&v, assign_text);
            std::printf("%d %d %s\n", v.flags, v.value, to_hex(v.text).c_str());
            std::free(v.text);
        } else if (kind[0] == 'P' || kind[0] == 'Q') {
            int flags = 0, value = 0;
            char text[4096];
            if (std::fscanf(in, "%d %d %4095s", &flags, &value, text) != 3) break;
            std::memset(g_world, 0, sizeof g_world);
            std::memset(g_stack, 0, sizeof g_stack);
            g_stack[0].flags = static_cast<uint8_t>(flags);
            g_stack[0].value = value;
            g_stack[0].text = dup_text(unhex(text));
            *world::script_count(g_world) = 1;
            *reinterpret_cast<sv::Value**>(g_world + world::SCRIPT_VALUES) = g_stack;
            if (kind[0] == 'P') {
                const int32_t result = world::PopInt(g_world);
                std::printf("%d %d -\n", result, *world::script_count(g_world));
            } else {
                char** result = world::PopString(g_world, assign_text);
                std::printf("%d %d %s\n", g_stack[0].flags, *world::script_count(g_world),
                            to_hex(*result).c_str());
            }
            std::free(g_stack[0].text);
        } else if (kind[0] == 'O' || kind[0] == 'V' || kind[0] == 'R') {
            int flags = 0, value = 0;
            char text[4096];
            if (std::fscanf(in, "%d %d %4095s", &flags, &value, text) != 3) break;
            g_log_errors = 0;
            g_log_writes = 0;
            setup_world(flags, value, unhex(text));
            if (kind[0] == 'O') {
                const int32_t result = world::PopObject(g_world, g_log);
                std::printf("O %d %d %d\n", result, *world::script_count(g_world),
                            g_log_errors);
            } else if (kind[0] == 'R') {
                const int32_t result =
                    world::ScriptPopObject(g_world + world::SCRIPT_MACHINE, g_log);
                std::printf("R %d %d %d\n", result,
                            *world::machine_count(g_world + world::SCRIPT_MACHINE),
                            g_log_errors);
            } else {
                void* result = world::PopVid(g_world, g_log, "for test", g_empty_vid);
                std::printf("V %d %d %d\n", static_cast<int32_t>(
                                                reinterpret_cast<uintptr_t>(result)),
                            *world::script_count(g_world), g_log_writes);
            }
            std::free(g_stack[0].text);
        } else {
            int op = 0, left_flags = 0, right_flags = 0;
            int left_value = 0, right_value = 0;
            char left_text[4096], right_text[4096];
            if (std::fscanf(in, "%d %d %d %4095s %d %d %4095s", &op, &left_flags, &left_value,
                            left_text, &right_flags, &right_value, right_text) != 7)
                break;
            sv::Value left{};
            sv::Value right{};
            left.flags = static_cast<uint8_t>(left_flags);
            left.value = left_value;
            left.text = dup_text(unhex(left_text));
            right.flags = static_cast<uint8_t>(right_flags);
            right.value = right_value;
            right.text = dup_text(unhex(right_text));
            const int32_t value =
                sv::BinaryOp(&left, op, &right, append_text, log_noop, nullptr);
            std::printf("%d %d %s\n", left.flags, value, to_hex(left.text).c_str());
            std::free(left.text);
            std::free(right.text);
        }
    }
    std::fclose(in);
    return 0;
}
