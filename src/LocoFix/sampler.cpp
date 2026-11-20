// A sampling profiler of the game's main thread (profile.h, prof::start_sampler): where a frame's
// time goes, down to the function - in the exe, in LocoFix and in the graphics layers under them
// (dgVoodoo, Direct3D, the driver).
//
// Off unless EditorProfile = 2 (or LOCOFIX_PROFILE=2): then, besides the profiler's lines, a thread
// of its own stops the main thread about every millisecond (SuspendThread / GetThreadContext /
// ResumeThread) and notes where it is: its instruction pointer, the first return address into the
// exe found on its stack (for time spent below the exe - in dgVoodoo or the driver - that is the
// engine's call that led there) and the view's width (the zoom). The notes go to logs\sampler.bin
// (12-byte records), the modules loaded to logs\sampler.txt; tools\profile_samples.py reads them
// with the names of re\symbols.csv and LocoFix's link map. Nothing is allocated, logged or loaded
// while the main thread is stopped (it may hold the heap's or the loader's lock).
#include "profile.h"
#include "patch.h"
#include <psapi.h>
#include <cstdio>
#include <intrin.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace prof {

namespace {

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr size_t G_VIEW_RIGHT = 0x224;
constexpr uintptr_t EXE_TEXT = 0x401000, EXE_TEXT_END = 0x479000;
constexpr size_t STACK_SCAN = 64 * 1024;
constexpr int RECORDS = 1 << 16;
constexpr DWORD FLUSH_MS = 2000;

#pragma pack(push, 1)
struct Record {
    uint32_t ip;
    uint32_t caller;   // the first return address into the exe on the stack, 0 when none
    uint16_t view;     // the view's width (logical pixels)
    uint16_t ms;       // the time since the sampler started, in units of 16 ms
};
#pragma pack(pop)

HANDLE g_main = nullptr;
uintptr_t g_stack_base = 0;
Record g_records[RECORDS];
int g_count = 0;
uint8_t g_stack[STACK_SCAN];

// A return address: the bytes before it are a call (E8 rel32, FF /2 with a register, a memory
// operand or a displacement).
bool after_call(uintptr_t a) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(a);
    if (p[-5] == 0xE8) return true;
    if (p[-2] == 0xFF && (p[-1] & 0x38) == 0x10) return true;
    if (p[-3] == 0xFF && (p[-2] & 0x38) == 0x10) return true;
    if (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10) return true;
    if (p[-7] == 0xFF && (p[-6] & 0x38) == 0x10) return true;
    return false;
}

void write_modules(FILE* f) {
    HMODULE modules[256];
    DWORD bytes = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof modules, &bytes)) return;
    for (DWORD i = 0; i < bytes / sizeof(HMODULE) && i < 256; ++i) {
        MODULEINFO info = {};
        char name[MAX_PATH] = {};
        K32GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof info);
        GetModuleFileNameA(modules[i], name, sizeof name);
        std::fprintf(f, "module %08X %08X %s\n", static_cast<unsigned>(reinterpret_cast<uintptr_t>(info.lpBaseOfDll)),
                     static_cast<unsigned>(info.SizeOfImage), name);
    }
}

DWORD WINAPI sampler(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x2 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    const std::string folder = patch::log_folder();
    FILE* bin = nullptr;
    fopen_s(&bin, (folder + "sampler.bin").c_str(), "wb");
    if (!bin) return 0;
    const DWORD start = GetTickCount();
    DWORD flushed = start, modules_written = 0;
    for (;;) {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;   // 1 ms
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep(1);
        }
        CONTEXT c = {};
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (SuspendThread(g_main) == static_cast<DWORD>(-1)) break;
        if (!GetThreadContext(g_main, &c)) {
            ResumeThread(g_main);
            continue;
        }
        size_t n = 0;
        if (c.Esp < g_stack_base) {
            n = g_stack_base - c.Esp;
            if (n > STACK_SCAN) n = STACK_SCAN;
            std::memcpy(g_stack, reinterpret_cast<const void*>(c.Esp), n);
        }
        const float view = *reinterpret_cast<const float*>(*reinterpret_cast<uint8_t* const*>(G_GRAPHICS) + G_VIEW_RIGHT);
        ResumeThread(g_main);
        Record& r = g_records[g_count++];
        r.ip = c.Eip;
        r.caller = 0;
        r.view = static_cast<uint16_t>(view);
        r.ms = static_cast<uint16_t>((GetTickCount() - start) / 16);
        if (c.Eip < EXE_TEXT || c.Eip >= EXE_TEXT_END)
            for (size_t i = 0; i + 4 <= n; i += 4) {
                const uint32_t a = *reinterpret_cast<const uint32_t*>(g_stack + i);
                if (a >= EXE_TEXT + 8 && a < EXE_TEXT_END && after_call(a)) {
                    r.caller = a;
                    break;
                }
            }
        const DWORD now = GetTickCount();
        if (g_count == RECORDS || now - flushed >= FLUSH_MS) {
            std::fwrite(g_records, sizeof(Record), g_count, bin);
            std::fflush(bin);
            g_count = 0;
            flushed = now;
            if (now - modules_written > 10000) {   // the modules, now and then (some load late)
                FILE* f = nullptr;
                if (!fopen_s(&f, (folder + "sampler.txt").c_str(), "w") && f) {
                    std::fprintf(f, "locofix %08X\n", static_cast<unsigned>(reinterpret_cast<uintptr_t>(&__ImageBase)));
                    write_modules(f);
                    std::fclose(f);
                }
                modules_written = now;
            }
        }
    }
    return 0;
}

}  // namespace

void start_sampler() {
    if (g_main) return;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
        return;
    g_stack_base = __readfsdword(4);   // NT_TIB.StackBase of this (the main) thread
    CloseHandle(CreateThread(nullptr, 0, &sampler, nullptr, 0, nullptr));
    patch::log("profiler: sampling the main thread into logs\\sampler.bin");
}

}  // namespace prof
