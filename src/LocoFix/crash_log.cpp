// The crash log: where an exception happened, in LocoFix.log.
//
// The engine's own unhandled-exception filter (World_Construct sets it, 0x419820) writes only an
// "EXCEPTION" line with the address to error.log and ends the game. A vectored handler of ours
// sees the exception first (first chance, before any handler of the engine, ours or the CRT):
// for the codes that are bugs (an access violation, a division by zero, an illegal instruction,
// a stack overflow...) it writes to LocoFix.log the code, the address with its module and
// offset (Locoland.exe, LocoFix.dll, DDraw.dll...), for an access violation the address read or
// written, the registers and the top of the stack with each value that points into a module's
// code named the same way - enough to find the function in re/symbols.csv. It does not handle
// anything (EXCEPTION_CONTINUE_SEARCH): the game goes on as without it. At most
// MAX_REPORTS exceptions a run are written, as a handled one could repeat.
#include "patch.h"

#include <cstdio>

namespace {

constexpr int MAX_REPORTS = 4;
constexpr int STACK_WORDS = 24;
LONG g_reports = 0;

bool is_bug(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case STATUS_HEAP_CORRUPTION:
    case STATUS_STACK_BUFFER_OVERRUN:
        return true;
    default:
        return false;
    }
}

// "Module+offset" of an address in a loaded module's image, else "".
void module_of(uintptr_t address, char* out, size_t size) {
    out[0] = '\0';
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(address), &module) || !module)
        return;
    char path[MAX_PATH];
    if (!GetModuleFileNameA(module, path, MAX_PATH)) return;
    const char* name = std::strrchr(path, '\\');
    name = name ? name + 1 : path;
    std::snprintf(out, size, "%s+%X", name, static_cast<unsigned>(address - reinterpret_cast<uintptr_t>(module)));
}

bool in_code(uintptr_t address) {
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery(reinterpret_cast<void*>(address), &m, sizeof m) || m.State != MEM_COMMIT) return false;
    return (m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0 &&
           m.Type == MEM_IMAGE;
}

LONG CALLBACK log_exception(EXCEPTION_POINTERS* e) {
    const EXCEPTION_RECORD* r = e->ExceptionRecord;
    if (!is_bug(r->ExceptionCode) || InterlockedIncrement(&g_reports) > MAX_REPORTS) return EXCEPTION_CONTINUE_SEARCH;
    const CONTEXT* c = e->ContextRecord;
    const uintptr_t at = reinterpret_cast<uintptr_t>(r->ExceptionAddress);
    char where[MAX_PATH + 16];
    module_of(at, where, sizeof where);
    char access[64] = "";
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2)
        std::snprintf(access, sizeof access, ", %s %08X",
                      r->ExceptionInformation[0] == 1 ? "writing" : r->ExceptionInformation[0] == 8 ? "executing" : "reading",
                      static_cast<unsigned>(r->ExceptionInformation[1]));
    patch::log("crash: exception %08lX at %08X (%s)%s, first chance, thread %lu",
               static_cast<unsigned long>(r->ExceptionCode), static_cast<unsigned>(at), where, access,
               static_cast<unsigned long>(GetCurrentThreadId()));
    patch::log("crash: eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX",
               c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    // the top of the stack; a value pointing into a module's code is likely a return address
    const auto* stack = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(c->Esp));
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(stack, &m, sizeof m) && m.State == MEM_COMMIT) {
        const uintptr_t end = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
        std::string line;
        for (int i = 0; i < STACK_WORDS && reinterpret_cast<uintptr_t>(stack + i + 1) <= end; ++i) {
            char word[MAX_PATH + 32];
            const uint32_t v = stack[i];
            char name[MAX_PATH + 16] = "";
            if (in_code(v)) module_of(v, name, sizeof name);
            std::snprintf(word, sizeof word, name[0] ? " %08X(%s)" : " %08X", static_cast<unsigned>(v), name);
            line += word;
            if (i % 8 == 7) {
                patch::log("crash: stack+%02X:%s", (i - 7) * 4, line.c_str());
                line.clear();
            }
        }
        if (!line.empty()) patch::log("crash: stack:%s", line.c_str());
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void install_crash_log() {
    if (AddVectoredExceptionHandler(1, &log_exception)) patch::log("crash log: on");
}
