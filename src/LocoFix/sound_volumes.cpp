// The player's sound and music volumes in a game that does not start at the main menu (BUGS.md F33),
// and the volumes the engine plays at in LocoFix.log at every level load.
//
// The engine keeps both volumes in the sound object: g_sound +0x3c4 the effects (0-100, Sound_Construct
// sets 100) and +0x3c8 the music (0-100; Sound_Construct sets -1, "not set", which SoundBuffer_Process
// plays as 100). It has no setting of its own for them: the scripts keep the player's choice, 0-29 in
// the settings page's steps, as "sound" and "music" in the Players key (player_registry.cpp moves it to
// HKCU), and only the main menu applies it - menu.lgc's main() reads both with GetReg as it starts (a
// missing value is "29", one outside 0-29 is 14) and calls SetSound(v*100/29) and SetMusic(v*100/29)
// (obsolete.lgc: SetSoundVolume 130, SetMusicVolume 131). The Esc menu's settings page (menutact.lgc)
// sets them only when one is changed there.
// A game that starts on a map never runs the menu, so it played everything at 100: the original's
// command line map (World_Construct's levelFile), and with LocoFix every game the editor starts -
// the editor itself (--editor --test-map), File > Play the map, the way back to the editor
// (restart.cpp) - with Run the level and every map loaded in it, until the player touched a volume
// on the settings page. So before the first level of a game loads, the volumes are read from the
// Players key and set the menu's way, through the engine's own setters; the menu, when the game
// starts there, sets the same values again a moment later.
#include "patch.h"

#include <cstdlib>

namespace {

constexpr uintptr_t G_SOUND = 0x5d3a54;   // g_sound: the sound object
constexpr size_t SOUND_EFFECTS_VOLUME = 0x3c4, SOUND_MUSIC_VOLUME = 0x3c8;
constexpr uintptr_t SET_SOUND_VOLUME = 0x444f90, SET_MUSIC_VOLUME = 0x444fc0;   // test.cpp's quiet run hooks both
constexpr char PLAYERS_KEY[] = "SOFTWARE\\Gromada\\Locoland\\Players";
constexpr int STEPS = 29;   // the settings page's bar: 0-29

using SetVolume_t = void(__fastcall*)(void* sound, void* edx, int volume);
bool g_applied = false;

// menu.lgc: v = GetReg(PLAYERS, name, "29"); if (v > 29 || v < 0) v = 14; the int of a text is atoi's.
int saved_step(const char* name, char* text, DWORD text_size) {
    DWORD size = text_size;
    if (RegGetValueA(HKEY_CURRENT_USER, PLAYERS_KEY, name, RRF_RT_REG_SZ, nullptr, text, &size) != ERROR_SUCCESS)
        lstrcpynA(text, "29", text_size);
    const int step = std::atoi(text);
    return step > STEPS || step < 0 ? 14 : step;
}

void apply_saved_volumes(uint8_t* sound) {
    char sound_text[32] = "", music_text[32] = "";
    const int sound_step = saved_step("sound", sound_text, sizeof sound_text);
    const int music_step = saved_step("music", music_text, sizeof music_text);
    reinterpret_cast<SetVolume_t>(SET_SOUND_VOLUME)(sound, nullptr, sound_step * 100 / STEPS);
    reinterpret_cast<SetVolume_t>(SET_MUSIC_VOLUME)(sound, nullptr, music_step * 100 / STEPS);
    patch::log("sound volumes: the player's from the Players key before the first level - sound '%s' (step %d), "
               "music '%s' (step %d)", sound_text, sound_step, music_text, music_step);
}

}  // namespace

// frame_limit.cpp's Level_Load wrapper, before a level loads.
void sound_volumes_level_loading(const char* map) {
    auto* sound = *reinterpret_cast<uint8_t**>(G_SOUND);
    if (!sound) return;
    if (!g_applied) {
        g_applied = true;
        apply_saved_volumes(sound);
    }
    const int effects = *reinterpret_cast<const int*>(sound + SOUND_EFFECTS_VOLUME);
    const int music = *reinterpret_cast<const int*>(sound + SOUND_MUSIC_VOLUME);
    char music_text[32];
    if (music < 0) lstrcpynA(music_text, "not set (plays at 100)", sizeof music_text);
    else wsprintfA(music_text, "%d", music);
    patch::log("sound volumes at the load of %s: effects %d, music %s", map ? map : "?", effects, music_text);
}
