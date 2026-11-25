// The map's script compiled before the editor runs or plays the map (script_check.cpp).
#pragma once
#include <string>

struct ScriptCheck {
    bool ok = true;           // it compiles (or could not be checked)
    bool missing = false;     // the file is not there
    std::string file;         // the .lgc checked
    std::string line;         // the compiler's first error, as error.log has it (without the time)
    std::string near_text;    // the 60 characters of source around it, as error.log shows them
};

// Compiles `lgc` with the engine's compiler as Level_Load would, apart from the level: whether the
// level's load would get its script or end the game.
ScriptCheck script_check(const std::string& lgc);

// After a level's load in the editor: true, with why, when it is held without its script, which
// does not compile (the editor's load of such a map closed the game before). Once a load.
bool script_check_held(ScriptCheck& check);
