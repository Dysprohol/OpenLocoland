// The sound and music volumes the engine plays at, in LocoFix.log at every level load.
//
// The engine keeps both in the sound object: g_sound +0x3c4 the effects (0-100, Sound_Construct sets
// 100) and +0x3c8 the music (0-100; Sound_Construct sets -1, "not set", which SoundBuffer_Process
// plays as 100). Only the scripts change them (SetSoundVolume 130 and SetMusicVolume 131 through
// obsolete.lgc's SetSound / SetMusic); the player's choice, 0-29 in the settings page's steps, is kept
// as "sound" and "music" in the Players key.
#include "patch.h"

namespace {

constexpr uintptr_t G_SOUND = 0x5d3a54;   // g_sound: the sound object
constexpr size_t SOUND_EFFECTS_VOLUME = 0x3c4, SOUND_MUSIC_VOLUME = 0x3c8;

}  // namespace

// frame_limit.cpp's Level_Load wrapper, before a level loads.
void sound_volumes_level_loading(const char* map) {
    const auto* sound = *reinterpret_cast<const uint8_t* const*>(G_SOUND);
    if (!sound) return;
    const int effects = *reinterpret_cast<const int*>(sound + SOUND_EFFECTS_VOLUME);
    const int music = *reinterpret_cast<const int*>(sound + SOUND_MUSIC_VOLUME);
    char music_text[32];
    if (music < 0) lstrcpynA(music_text, "not set (plays at 100)", sizeof music_text);
    else wsprintfA(music_text, "%d", music);
    patch::log("sound volumes at the load of %s: effects %d, music %s", map ? map : "?", effects, music_text);
}
