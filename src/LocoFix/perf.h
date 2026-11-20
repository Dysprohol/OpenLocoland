// LocoFix's cheaper ways of drawing a frame (docs/dev/changes.md, the fixes from 54 on). Each draws
// exactly what the engine's own code draws. In test mode, the test command 40053 (test.cpp) switches
// them all back to the engine's own code and again, so the same frame can be taken both ways and
// compared pixel by pixel.
#pragma once

namespace perf {
inline bool g_original = false;   // true: the engine's own code (test mode only)
}
