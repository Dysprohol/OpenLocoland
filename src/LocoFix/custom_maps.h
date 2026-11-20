// The maps of the custom maps page (custom_maps.cpp): found in maps\ by themselves, their names,
// renaming and deleting them.
#pragma once
#include <cstdint>
#include <string>

// What is wrong with `name` as the name of a new map - its folder in maps\ and its files
// <name>.map ... - in the pack's words, or empty when it can be used.
std::string map_name_problem(const std::string& name);
// The rules in one sentence (the dialogs show it).
const char* map_name_rule();
// maps\<folder>\<folder>.map is there (any case).
bool custom_map_exists(const std::string& folder);
// A folder of the game or of the editor (the campaign's lNN, script, outro, POLIGON, the template,
// the scratch _new..., _run): never renamed, deleted or overwritten.
bool game_map_folder(const std::string& folder);
// A custom map has one name: its folder's. Its title - on the page, the script's "// title:" and
// BRIEF_HEADER - is that name in capitals (custom_map_name_title). Not for the game's folders:
// POLIGON, the template and the campaign keep the titles of their scripts.
bool custom_map_named(const std::string& folder);
// The title of the name `name`: ASCII capitals without quotes, at most 24 characters.
std::string custom_map_name_title(const std::string& name);
// A map's script under the name `name`: its "// title:" and BRIEF_HEADER lines become the name's
// title (editor_exchange.cpp renames and imports every map this way).
std::string custom_map_retitled(const std::string& script, const std::string& name);
// The title the page shows for it (its name's; POLIGON's from its script).
std::string custom_map_title(const std::string& folder);
// Looks through maps\ and writes maps\custom.lst for the menu (only when it changed).
void custom_maps_list();
// Why the map of the folder cannot be opened now, in the pack's words (capitals, for the menu's
// font: `menu`) - empty when it can.
std::string custom_map_problem(const std::string& folder, bool menu);
// maps\<from>\ copied as maps\<to>\ with its files and paths renamed (as an import under a new name
// does); the old folder goes to the Recycle Bin when `remove_old` and the new one is complete.
// False with `error` (the pack's words): nothing changed then.
bool custom_map_rename(const std::string& from, const std::string& to, bool remove_old, std::string& error);
// maps\<folder>\ to the Recycle Bin, and off the page. False with `error`.
bool custom_map_remove(const std::string& folder, std::string& error);
// The maps whose ground image lies in maps\<folder>\ (saved from that map) get a copy of it in
// their own folder, so they keep it when the folder is renamed or deleted.
void custom_maps_keep_grounds(const std::string& folder);
// frame_limit.cpp's Level_Load wrapper: a map that is not there - the main menu instead (the game
// would stay on a black screen).
const char* custom_maps_level_guard(const char* map);
// restart.cpp's hook of Script_CallExtern: the externs of the custom maps page (204-206); false
// when `function` is not one of them.
bool custom_maps_extern(uint8_t* world, int function);
