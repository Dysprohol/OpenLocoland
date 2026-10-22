// Script Log(text) (extern 154) passes the script text as the printf format string.
//
// In Script_CallExtern the case 0x9a at 0x4405d1 pops the argument, converts it to a
// String and calls Log_Write(g_log, text):
//
//   4405ec: call ScriptValue_ToString
//   4405f1: mov eax,[eax]        ; text
//   4405f3: mov ecx,[0x4a9d5c]   ; g_log
//   4405f9: push eax             ; format = text
//   4405fa: push ecx             ; log
//   4405fb: call Log_Write       ; vsprintf(buffer, format, ...)
//   440600: add esp,8
//
// Log_Write runs the text through vsprintf, so a '%' in a logged string reads whatever
// is on the stack: "%s" dereferences a random pointer, "%n" writes to one. A mission
// logging e.g. "100%" prints garbage at best and crashes at worst.
//
// The fix replaces the call with a trampoline that pushes the text as an argument and
// passes a literal "%s" as the format: Log_Write(g_log, "%s", text).
#include "patch.h"

namespace {

constexpr uintptr_t LOG_CALL = 0x4405fb;   // call Log_Write in the Log case
constexpr uintptr_t LOG_WRITE = 0x426080;  // Log_Write(log, format, ...)
constexpr uintptr_t AFTER_CALL = 0x440600; // add esp,8

const char kFormat[] = "%s";

}  // namespace

void install_script_log() {
    // call Log_Write
    const uint8_t expected[] = {0xE8, 0x80, 0x5A, 0xFE, 0xFF};
    if (!patch::check(LOG_CALL, expected, sizeof expected)) {
        patch::log("skipped script Log fix @ %08X: unexpected bytes",
                   static_cast<unsigned>(LOG_CALL));
        return;
    }

    auto* tramp = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        patch::log("skipped script Log fix: VirtualAlloc failed");
        return;
    }

    size_t n = 0;
    const auto here = [&] { return reinterpret_cast<uintptr_t>(tramp) + n; };

    // On entry the stack is [g_log][text] (text pushed first, then the log). Push the
    // arguments for Log_Write(log, "%s", text), leaving the original two in place.
    tramp[n++] = 0xFF; tramp[n++] = 0x74; tramp[n++] = 0x24; tramp[n++] = 0x04;  // push [esp+4]  (text)
    tramp[n++] = 0x68;                                   // push kFormat
    const uintptr_t fmt = reinterpret_cast<uintptr_t>(kFormat);
    std::memcpy(tramp + n, &fmt, 4); n += 4;
    tramp[n++] = 0xFF; tramp[n++] = 0x74; tramp[n++] = 0x24; tramp[n++] = 0x08;  // push [esp+8]  (g_log)
    tramp[n++] = 0xE8;                                   // call Log_Write
    int32_t rel = static_cast<int32_t>(LOG_WRITE - (here() + 4));
    std::memcpy(tramp + n, &rel, 4); n += 4;
    tramp[n++] = 0x83; tramp[n++] = 0xC4; tramp[n++] = 0x0C;  // add esp,12 (our three arguments)
    tramp[n++] = 0xE9;                                   // jmp AFTER_CALL (drops the original two)
    rel = static_cast<int32_t>(AFTER_CALL - (here() + 4));
    std::memcpy(tramp + n, &rel, 4); n += 4;

    uint8_t jump[5];
    jump[0] = 0xE9;
    rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(tramp) - (LOG_CALL + 5));
    std::memcpy(jump + 1, &rel, 4);
    patch::write(LOG_CALL, jump, sizeof jump);
    patch::log("script Log fix installed @ %08X (\"%%s\" format)", static_cast<unsigned>(LOG_CALL));
}
