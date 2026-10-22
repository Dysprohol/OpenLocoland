// Safe map saving: Level_Save (0x41e0b0, World vtable slot 7, script Save(filename), the
// editor's File > Save) writes the level as a .map file the game loads again. Its problems
// (BUGS.md F16-F20):
//  - saving over the loaded map (World->levelFile) first renamed that file to "tmp_del!.map"
//    in the current directory, because the OBJ chunk is copied from it after the new file is
//    created; when the new file could not be created, levelFile stayed "tmp_del!.map" and the
//    map was left under that name,
//  - that check compared the names case-sensitively, so the same file under another spelling
//    ("Maps\..." for "maps\...") was truncated by ResFile_Create before OBJ was copied from
//    it - the saved map lost its own VIDs (the ground),
//  - an existing target was truncated before anything was written, so a failed save lost it,
//  - with a ground the engine made itself (World_CreateEmptyGround: a map without VID 1024
//    that has decals, imageFlags 0x225 like a level VID) it still copied OBJ from levelFile,
//    which does not describe that ground.
//
// Here every save goes to "<target>.saving" next to the target - never the name of the loaded
// map, so the rename to tmp_del!.map is never reached and OBJ is copied from the untouched
// loaded map - and replaces the target only when that file was written (MoveFileEx). When the
// level has VIDs of its own and the map they come from is gone, nothing is saved. The
// self-made ground loses its level flag 0x200 for the save, so it does not make Level_Save
// copy OBJ.
//
// The OBJ chunk itself came out wrong too: ResFile_CopyChunk (0x4370b0, used only here) read
// the chunk's whole size (the 12-byte chunk header and the 4-byte record size counted in it)
// from the first record's data on, so every save appended the 16 bytes that follow the record
// in the source - the pad byte and the next chunk's header - to the OBJ record, and a chunk of
// several records would have become one record holding them all. It is rewritten here to copy
// record by record with the game's own ResFile functions, so OBJ comes out as it was read.
//
// Not a bug, but changed as well: HEAD gets the game time (g_gameTime) - the shipped maps carry
// the time of the authors' editor session (93368 in l01), which the loader ignores - so two
// saves of the same map differed. It is written as 0 now (the push at 0x41e40c takes a zero of
// ours), so a map saved twice gives the same file.
//
// Level_Save takes the name as a String by value and frees it, like Level_Load, so the name
// handed on is a String of the game's own heap (crt_new) and the caller's is freed here.
#include "patch.h"

namespace {

constexpr uintptr_t LEVEL_SAVE = 0x41e0b0;
constexpr uintptr_t PUSH_GAME_TIME = 0x41e40c;   // Level_Save: push &g_gameTime (HEAD time)
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;
constexpr uintptr_t SELF_MADE_GROUND_NAME = 0x47f964;   // "Self Created Hardware Prerendered..."
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr uintptr_t CRT_DELETE = 0x456a00;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;
constexpr size_t WORLD_LEVEL_FILE = 0x014;
constexpr size_t WORLD_HWND = 0x214;
constexpr size_t WORLD_VID_COUNT = 0x290;
constexpr size_t WORLD_VIDS = 0x294;
constexpr size_t VID_NAME = 0x008;
constexpr size_t VID_IMAGE_FLAGS = 0x2a0;
constexpr uint16_t LEVEL_VID = 0x200;   // imageFlags of VIDs loaded from a map's OBJ chunk
constexpr char SAVING_SUFFIX[] = ".saving";

using New_t = char*(__cdecl*)(size_t size);
using Delete_t = void(__cdecl*)(const void* memory);
using LevelSave_t = void(__fastcall*)(uint8_t* world, void* edx, char* name);
LevelSave_t g_level_save = nullptr;

// ---- ResFile_CopyChunk ----

constexpr uintptr_t COPY_CHUNK = 0x4370b0;
constexpr uintptr_t RES_FIND_FIRST = 0x436ac0;
constexpr uintptr_t RES_NEXT_RECORD = 0x436c30;
constexpr uintptr_t RES_BEGIN_RECORD = 0x436ca0;
constexpr uintptr_t RES_END_RECORD = 0x436d50;
constexpr size_t RES_RECORD_SIZE = 0x30;
constexpr size_t RES_FILE = 0x38;

using ResFind_t = int(__fastcall*)(uint8_t* res, void* edx, uint32_t id);
using ResBegin_t = int(__fastcall*)(uint8_t* res, void* edx, uint32_t id, int rewrite);
using ResEnd_t = void(__fastcall*)(uint8_t* res, void* edx);
using ResIo_t = int(__fastcall*)(uint8_t* res, void* edx, void* buffer, int size);   // vtable Read, Write

ResIo_t io(uint8_t* res, int slot) { return (*reinterpret_cast<ResIo_t**>(res))[slot]; }

// ResFile_CopyChunk(this = target, source, id): every record of the chunks with the id, each
// as a record of its own. Returns 0, or 1 when a file is not open (as the original).
int __fastcall CopyChunk_by_record(uint8_t* target, void*, uint8_t* source, uint32_t id) {
    if (!*reinterpret_cast<void**>(target + RES_FILE) || !*reinterpret_cast<void**>(source + RES_FILE)) return 1;
    // positioned on the first record's data, its size read
    if (reinterpret_cast<ResFind_t>(RES_FIND_FIRST)(source, nullptr, id) != 0) return 0;
    std::string buffer;
    do {
        const int size = *reinterpret_cast<int*>(source + RES_RECORD_SIZE);
        buffer.resize(size > 0 ? size : 0);
        io(source, 0)(source, nullptr, &buffer[0], static_cast<int>(buffer.size()));
        reinterpret_cast<ResBegin_t>(RES_BEGIN_RECORD)(target, nullptr, id, 0);
        io(target, 1)(target, nullptr, &buffer[0], static_cast<int>(buffer.size()));
        reinterpret_cast<ResEnd_t>(RES_END_RECORD)(target, nullptr);
    } while (reinterpret_cast<ResFind_t>(RES_NEXT_RECORD)(source, nullptr, id) == 0);
    return 0;
}

const uint32_t g_zero_time = 0;
bool g_last_save_ok = false;

char* game_string(const std::string& text) {
    char* s = reinterpret_cast<New_t>(CRT_NEW)(text.size() + 1);
    if (s) std::memcpy(s, text.c_str(), text.size() + 1);
    return s;
}

void free_game_string(char* s) {
    if (s && reinterpret_cast<uintptr_t>(s) != EMPTY_STRING) reinterpret_cast<Delete_t>(CRT_DELETE)(s);
}

bool exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

uint8_t* vid(uint8_t* world, int n) { return reinterpret_cast<uint8_t**>(world + WORLD_VIDS)[n]; }
uint16_t& image_flags(uint8_t* v) { return *reinterpret_cast<uint16_t*>(v + VID_IMAGE_FLAGS); }

bool self_made_ground(uint8_t* v) {
    const char* name = *reinterpret_cast<const char**>(v + VID_NAME);
    return name && lstrcmpA(name, reinterpret_cast<const char*>(SELF_MADE_GROUND_NAME)) == 0;
}

void __fastcall LevelSave_safe(uint8_t* world, void* edx, char* name) {
    g_last_save_ok = false;
    if (!name || !*name) {   // the original does nothing then, but frees the name
        g_level_save(world, edx, name);
        return;
    }
    const std::string target = name;
    free_game_string(name);
    const char* level = *reinterpret_cast<const char**>(world + WORLD_LEVEL_FILE);

    // VIDs of the level itself (the ground) are copied from the OBJ chunk of the loaded map;
    // the self-made ground is not described there.
    bool copies_obj = false;
    uint8_t* self_made = nullptr;
    const int count = *reinterpret_cast<int*>(world + WORLD_VID_COUNT);
    for (int i = 0; i < count; ++i) {
        uint8_t* v = vid(world, i);
        if (!v || !(image_flags(v) & LEVEL_VID)) continue;
        if (self_made_ground(v)) self_made = v;
        else copies_obj = true;
    }
    if (copies_obj && !(level && exists(level))) {
        patch::log("level save: %s not saved - its VIDs come from %s, which is not there", target.c_str(),
                   level ? level : "(no map)");
        MessageBoxA(*reinterpret_cast<HWND*>(world + WORLD_HWND), "The map was not saved: the map file it was loaded from is missing.", "Save",
                    MB_OK | MB_ICONERROR);
        return;
    }

    const std::string temp = target + SAVING_SUFFIX;
    DeleteFileA(temp.c_str());
    char* temp_name = game_string(temp);
    if (!temp_name) return;
    if (self_made) image_flags(self_made) &= ~LEVEL_VID;
    g_level_save(world, edx, temp_name);   // frees temp_name
    if (self_made) image_flags(self_made) |= LEVEL_VID;

    if (!exists(temp)) {   // ResFile_Create failed; the game has shown "Can't open file"
        patch::log("level save: %s could not be written", temp.c_str());
        return;
    }
    if (!MoveFileExA(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileA(temp.c_str());
        patch::log("level save: %s not replaced (error %lu), the old file stays", target.c_str(), error);
        MessageBoxA(*reinterpret_cast<HWND*>(world + WORLD_HWND), "The map could not be saved under that name (is the file in use or read-only?).",
                    "Save", MB_OK | MB_ICONERROR);
        return;
    }
    g_last_save_ok = true;
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    GetFileAttributesExA(target.c_str(), GetFileExInfoStandard, &data);
    patch::log("level save: %s (%lu bytes%s)", target.c_str(), data.nFileSizeLow,
               self_made ? ", self-made ground not copied" : "");
}

}  // namespace

// Whether the last Level_Save wrote its file (editor.cpp reports it).
bool level_save_succeeded() { return g_last_save_ok; }

void install_level_save() {
    const uint8_t prologue[] = {0x6A, 0xFF, 0x68, 0xE4, 0x30, 0x47, 0x00};   // push -1 / push 0x4730e4
    uint8_t push_time[5] = {0x68};                                           // push 0x4a9d64
    const uint32_t game_time = G_GAME_TIME;
    std::memcpy(push_time + 1, &game_time, 4);
    const char self_made_text[] = "Self Created";
    if (!patch::check(PUSH_GAME_TIME, push_time, sizeof push_time) ||
        !patch::check(SELF_MADE_GROUND_NAME, self_made_text, sizeof self_made_text - 1)) {
        patch::log("skipped level save fixes: unexpected bytes");
        return;
    }
    g_level_save = reinterpret_cast<LevelSave_t>(
        patch::hook(LEVEL_SAVE, prologue, sizeof prologue, reinterpret_cast<void*>(&LevelSave_safe), "Level_Save"));
    if (!g_level_save) return;
    const uint32_t zero = reinterpret_cast<uint32_t>(&g_zero_time);
    std::memcpy(push_time + 1, &zero, 4);
    patch::write(PUSH_GAME_TIME, push_time, sizeof push_time);

    const uint8_t copy_chunk[] = {0x53, 0x55, 0x56, 0x57, 0x8B, 0xF9};   // push ebx/ebp/esi/edi / mov edi,ecx
    patch::redirect(COPY_CHUNK, copy_chunk, sizeof copy_chunk, reinterpret_cast<void*>(&CopyChunk_by_record),
                    "ResFile_CopyChunk");
}
