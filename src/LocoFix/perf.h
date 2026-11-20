// LocoFix's cheaper ways of drawing a frame (docs/dev/changes.md, the fixes from 54 on). Each draws
// exactly what the engine's own code draws. In test mode, the test command 40053 (test.cpp) switches
// them all back to the engine's own code and again, so the same frame can be taken both ways and
// compared pixel by pixel; 40056 switches only the fixes from 56 on (the locks of the picture), so their
// part can be timed apart from the lights' (54, 55); 40057 only the fixes from 60 on (the third round).
#pragma once

namespace perf {
inline bool g_original = false;   // true: the engine's own code (test mode only)
inline bool g_locks_original = false;   // true: the engine's own code for the fixes from 56 on (test mode only)
inline bool g_round3_original = false;   // true: the engine's own code for the fixes from 60 on (test mode only)
inline bool off(int fix) { return g_original || (fix >= 56 && g_locks_original) || (fix >= 60 && g_round3_original); }
}
