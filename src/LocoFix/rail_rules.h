// The rules of the rail track - how the engine links rails (Rail_Action, see DEVELOPERS.md "Trains
// and rails" and MAP_RULES.md 3) - for the editor's track tool (editor_track.cpp).
//
// The same rules as tools/map_tool.py (rail_candidates, rail_ends_at, plan_track,
// track_cells); tools/test_rail_rules.py builds this header into a test host and compares the
// two over generated cases, so they do not drift apart. Plain C++ without the game or Windows.
//
// A rail (VID 610) is a sprite 71 x 50 with 12 shapes - 4 straight rails and 8 curves - each
// drawn by a direction byte (DIRECTION) and going from one end to the other (ENDS, in halves of
// the sprite: 35.5 px in x, 25 px in y). The engine links two rails only where their end
// vectors are opposite and their points stand at the same height, so a turn needs a curve. The
// frames 12-35 of the rail are ramps: the same shapes with one point raised (rail_of). A track
// is laid along rail-network points, the places where two rails meet; every step is one rail.
#pragma once
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rail {

struct Vec {
    int x, y;
    bool operator==(const Vec& o) const { return x == o.x && y == o.y; }
    Vec opposite() const { return {-x, -y}; }
};

struct Point {
    double x, y;
};

constexpr double HALF_X = 35.5, HALF_Y = 25.0;   // half the rail sprite (71 x 50)
constexpr int SHAPES = 12;
constexpr int MAX_LINKS = 6;                      // links of one rail point ("Too many links")
constexpr int GRID_CELL = 8;

constexpr Vec ENDS[SHAPES][2] = {{{0, -1}, {0, 1}},  {{1, 0}, {-1, 0}},  {{1, -1}, {-1, 1}}, {{-1, -1}, {1, 1}},
                                 {{1, -1}, {0, 1}},  {{-1, -1}, {0, 1}}, {{1, 1}, {0, -1}},  {{-1, 1}, {0, -1}},
                                 {{1, 1}, {-1, 0}},  {{1, -1}, {-1, 0}}, {{-1, -1}, {1, 0}}, {{-1, 1}, {1, 0}}};
// the direction byte of each flat shape (frames 0-11, see rail_of)
constexpr int DIRECTION[SHAPES] = {0, 6, 13, 20, 27, 34, 41, 48, 55, 62, 69, 76};
constexpr int FRAMES = 37;                  // nDir of VID 610
constexpr int HALF_STEP = 128 / FRAMES;     // Vid.dirHalfStep
constexpr int ALTITUDE = 10;                // the rail points stand this much above the rail
constexpr int RAMP = 11;                    // sizeZ of VID 610: a ramp raises one point by it
constexpr int ANY_Z = INT_MIN;              // plan(): the rails at any height

struct Rail {   // a rail of the map: its centre, shape and the heights of its two points
    double x, y;
    int shape;
    int z0 = 0, z1 = 0;
};

// A rail object as Rail_Action puts its points (map_tool.py rail_of): the frame of the direction,
// ((direction + dirHalfStep) & 255) * nDir >> 8, gives the shape (frame % 12) and the heights -
// z + ALTITUDE; frames 12-23 raise the point of end 0 by RAMP, frames 24-35 that of end 1 (the
// engine truncates the heights to ints).
inline Rail rail_of(double x, double y, double z, int direction) {
    const int frame = ((direction + HALF_STEP) & 255) * FRAMES >> 8, group = frame / 12;
    return {x, y, frame % 12, static_cast<int>(z + ALTITUDE + (group == 1 ? RAMP : 0)),
            static_cast<int>(z + ALTITUDE + (group == 2 ? RAMP : 0))};
}

inline Point end_of(const Rail& r, int i) {
    return {r.x + ENDS[r.shape][i].x * HALF_X, r.y + ENDS[r.shape][i].y * HALF_Y};
}

inline int height_of(const Rail& r, int i) { return i == 0 ? r.z0 : r.z1; }

// map_tool.py compares rounded to 3 decimals (rail_candidates) or within 0.01 (rail_ends_at)
inline double round3(double v) { return std::round(v * 1000.0) / 1000.0; }
inline bool same3(double a, double b) { return round3(a) == round3(b); }
inline bool close(double a, double b) { return std::fabs(a - b) < 0.01; }

struct Candidate {
    double x, y;
    int shape;
    Vec first, second;   // the rail's end vectors at a and at b
};

// The rails that connect the rail ends a and b, in map_tool's order (rail_candidates).
inline std::vector<Candidate> candidates(Point a, Point b) {
    std::vector<Candidate> out;
    for (int swap = 0; swap < 2; ++swap) {
        for (int shape = 0; shape < SHAPES; ++shape) {
            const Vec first = ENDS[shape][swap], second = ENDS[shape][1 - swap];
            const double x = a.x - first.x * HALF_X, y = a.y - first.y * HALF_Y;
            if (round3(x + second.x * HALF_X) == b.x && round3(y + second.y * HALF_Y) == b.y)
                out.push_back({x, y, shape, first, second});
        }
    }
    return out;
}

// The end vectors of the rails that end at a point with their point there at the height z, or at
// any height with ANY_Z (rail_ends_at).
inline std::vector<Vec> ends_at(const std::vector<Rail>& rails, Point p, int z = ANY_Z) {
    std::vector<Vec> out;
    for (const Rail& r : rails)
        for (int i = 0; i < 2; ++i) {
            const Point e = end_of(r, i);
            if (close(e.x, p.x) && close(e.y, p.y) && (z == ANY_Z || height_of(r, i) == z))
                out.push_back(ENDS[r.shape][i]);
        }
    return out;
}

// A rail point the engine makes at a joint (rail_point_links): the rail ends there pointing the
// same way at the same height share it; it links to the far point of each of those rails (a rail
// laid twice adds no link) and to the point of the rails ending the opposite way at that height.
// The engine's limit of 6 links is per point, not per joint.
struct JointPoint {
    Vec end;
    int z;
    int links;
};

inline std::vector<JointPoint> point_links(const std::vector<Rail>& rails, Point p) {
    struct Far {
        Vec end;
        int z;
        double x, y;
        int far_z;
    };
    std::vector<Far> fars;
    std::vector<JointPoint> out;
    for (const Rail& r : rails)
        for (int i = 0; i < 2; ++i) {
            const Point e = end_of(r, i);
            if (!close(e.x, p.x) || !close(e.y, p.y)) continue;
            const Vec end = ENDS[r.shape][i], other = ENDS[r.shape][1 - i];
            const Far f = {end, height_of(r, i), round3(r.x + other.x * HALF_X / 2),
                           round3(r.y + other.y * HALF_Y / 2), height_of(r, 1 - i)};
            bool seen = false;
            for (const Far& g : fars)
                if (g.end == f.end && g.z == f.z && g.x == f.x && g.y == f.y && g.far_z == f.far_z) seen = true;
            if (seen) continue;
            fars.push_back(f);
            bool found = false;
            for (JointPoint& j : out)
                if (j.end == end && j.z == f.z) {
                    ++j.links;
                    found = true;
                }
            if (!found) out.push_back({end, f.z, 1});
        }
    std::vector<JointPoint> counted = out;
    for (JointPoint& j : counted)
        for (const JointPoint& k : out)
            if (k.end == j.end.opposite() && k.z == j.z) {
                ++j.links;
                break;
            }
    return counted;
}

inline bool contains(const std::vector<Vec>& v, Vec e) {
    for (const Vec& x : v)
        if (x == e) return true;
    return false;
}

enum class Error { None, Step, Join, Sharp, Last, Links, Duplicate };

inline const char* error_code(Error e) {
    switch (e) {
    case Error::None: return "ok";
    case Error::Step: return "step";
    case Error::Join: return "join";
    case Error::Sharp: return "sharp";
    case Error::Last: return "last";
    case Error::Links: return "links";
    case Error::Duplicate: return "duplicate";
    }
    return "?";
}

struct Segment {
    Point a, b;
    double x, y;   // the rail's centre
    int shape;
    Point end0, end1;
    int z0 = 0, z1 = 0;   // the heights of its points (end 0, end 1)
};

struct Plan {
    Error error = Error::None;
    int step = -1;   // the step it failed at (-1: links, or none)
    double turn = 0; // degrees, for Sharp
    bool crossing = false;   // `through`: the track's last point is a joint of other track it crosses
    std::vector<Segment> segments;
};

// The rails of a track along `points` next to `rails` (map_tool.py plan_track). `shapes` (one
// per step, or empty) picks the shape by hand; `z` is the height of the track's rail points (its
// rails' z + ALTITUDE): it joins only the rails whose points are at that height, ANY_Z all.
// `through`: the track goes on past its last point (the editor lays a line a rail a click), so a
// joint of other track there whose rails it neither joins nor shares a point with is crossed - as
// plan() lets a track cross such a joint between its first and last point - instead of refused.
// `lay` (the editor's Track tool, which lays what is clicked): only what the engine cannot take is
// refused - a point with more than MAX_LINKS links ("Too many links"), and a step off the lattice,
// which the tool never makes; a rail that joins nothing before it (trains stop there), a turn over 45
// degrees (they reverse), the end on a joint it does not join and a rail that is already there (the
// points are shared, no link is added) are laid. The rules still choose the shape a step means: one
// joining the rail before it when there is one. `ramp` (-1, 0, 1): each step's far point 11 lower or
// higher than its near one (the Track tool's PgDn / PgUp), the level going on from there.
inline Plan plan(const std::vector<Rail>& rails, const std::vector<Point>& points,
                 const std::vector<int>& shapes = {}, int z = ANY_Z, bool through = false, bool lay = false,
                 int ramp = 0) {
    Plan p;
    if (points.size() < 2) return p;
    if (z == ANY_Z) ramp = 0;
    std::vector<Vec> entry;
    for (const Vec& e : ends_at(rails, points[0], z)) {
        const Vec o = e.opposite();
        if (!contains(entry, o)) entry.push_back(o);
    }
    bool have_entry = !entry.empty();
    bool have_previous = false;
    double previous = 0;
    Vec second = {0, 0};
    int level = z;   // the height of the step's near point
    for (size_t n = 0; n + 1 < points.size(); ++n) {
        const Point a = points[n], b = points[n + 1];
        std::vector<Candidate> cs = candidates(a, b);
        const std::vector<Candidate> every = cs;   // `lay`: a shape given is laid joining or not
        if (cs.empty()) {
            p.error = Error::Step;
            p.step = static_cast<int>(n);
            return p;
        }
        if (have_entry) {
            std::vector<Candidate> joined;
            for (const Candidate& c : cs)
                if (contains(entry, c.first)) joined.push_back(c);
            if (joined.empty() && !lay) {
                p.error = Error::Join;
                p.step = static_cast<int>(n);
                return p;
            }
            if (!joined.empty()) cs = joined;
        }
        Candidate c = cs[0];
        if (!shapes.empty()) {
            bool found = false;
            for (const Candidate& k : cs)
                if (k.shape == shapes[n]) {
                    c = k;
                    found = true;
                    break;
                }
            for (const Candidate& k : every)
                if (!found && lay && k.shape == shapes[n]) {
                    c = k;
                    found = true;
                }
            if (!found) {
                p.error = Error::Step;
                p.step = static_cast<int>(n);
                return p;
            }
        }
        second = c.second;
        entry = {second.opposite()};
        have_entry = true;
        const double h = std::atan2(b.y - a.y, b.x - a.x) * 180.0 / 3.14159265358979323846;
        if (have_previous) {
            const double turn = std::fmod(std::fmod(h - previous + 180.0, 360.0) + 360.0, 360.0) - 180.0;
            if (std::fabs(turn) > 45.0 && !lay) {
                p.error = Error::Sharp;
                p.step = static_cast<int>(n);
                p.turn = turn;
                return p;
            }
        }
        previous = h;
        have_previous = true;
        // its points' heights: the near one at the level, the far one a ramp higher or lower
        const int far_z = level == ANY_Z ? ANY_Z : level + ramp * RAMP;
        const bool a_is_0 = c.first == ENDS[c.shape][0] && c.second == ENDS[c.shape][1];
        const int z0 = a_is_0 ? level : far_z, z1 = a_is_0 ? far_z : level;
        for (const Rail& r : rails)
            if (!lay && close(r.x, c.x) && close(r.y, c.y) && r.shape == c.shape && (z == ANY_Z || (r.z0 == z0 && r.z1 == z1))) {
                p.error = Error::Duplicate;
                p.step = static_cast<int>(n);
                return p;
            }
        const Rail made = {c.x, c.y, c.shape};
        Point e0 = end_of(made, 0), e1 = end_of(made, 1);
        e0 = {round3(e0.x), round3(e0.y)};
        e1 = {round3(e1.x), round3(e1.y)};
        p.segments.push_back({a, b, c.x, c.y, c.shape, e0, e1, z0, z1});
        level = far_z;
    }
    const std::vector<Vec> meeting = ends_at(rails, points.back(), level);
    if (!meeting.empty() && !contains(meeting, second.opposite())) {
        const bool cross = through && !contains(meeting, second);   // its point stays apart from theirs
        if (cross) {
            p.crossing = true;
        } else if (!lay) {
            p.error = Error::Last;
            p.step = static_cast<int>(points.size()) - 2;
            p.segments.clear();
            return p;
        }
    }
    // no rail point may get more links than the engine allows: at every joint of the track, with
    // the rails already there (ANY_Z: the rails at any height as if at one), each point at its height
    const bool any = z == ANY_Z;
    std::vector<Rail> laid;
    for (const Rail& r : rails) laid.push_back({r.x, r.y, r.shape, any ? 0 : r.z0, any ? 0 : r.z1});
    for (const Segment& s : p.segments) laid.push_back({s.x, s.y, s.shape, any ? 0 : s.z0, any ? 0 : s.z1});
    for (const Segment& s : p.segments)
        for (int i = 0; i < 2; ++i) {
            const Point q = i ? s.end1 : s.end0;
            const int height = any ? 0 : i ? s.z1 : s.z0;
            for (const JointPoint& j : point_links(laid, q))
                if (j.z == height && j.links > MAX_LINKS) {
                    p.error = Error::Links;
                    p.step = -1;
                    p.segments.clear();
                    return p;
                }
        }
    return p;
}

// ---- the joints of a track as the engine links them (map_tool.py track_report / check-track) ----
//
// What a rail end at a joint is for the trains, from the rails there (the rule of Rail_Action above:
// the rail ends that point opposite ways at the same height link, those pointing the same way at
// the same height share one point):
//  - Linked: a rail ends there the opposite way at the height of its point - the track goes on;
//  - Heights: rails end there the opposite way, but only at other heights - they do not link;
//  - Sharp: no rail the opposite way, but another rail ends there the same way at that height - the
//    two share one point and are linked, yet every way from one into the other turns more than 0x20
//    (RailDot_Search: a turn of 45 degrees or more is a reversal), so a train only turns round there
//    (a V; a joint of linked rails end to end never turns that much: the 12 shapes turn at most ~35
//    degrees at a point);
//  - DeadEnd: other rails end there, none the opposite way or the same way: at an angle the engine
//    does not link (a straight horizontal and a diagonal touching, say) - it looks joined and is not;
//  - Near: no other rail ends there, but the end lies within NEAR_MISS px of another rail's way (the line
//    between its ends, on the map) - a branch ending on the side of a track, an end a few px short;
//  - Loose: no other rail ends there and none is near: the end of a track (a siding, the main line
//    beyond the map's edge).
// A lone end that the engine links all the same - its look-out point (three quarters of the sprite
// out, truncated to whole px as RailMap_AddDot / RailMap_FindDot take it) within 2 px of a point of
// another rail at the same height, or the other's look-out at its point - is Linked: rails moved by
// hand off the lattice still link within 2 px.
enum class End { Linked, Heights, Sharp, DeadEnd, Near, Loose };

constexpr double NEAR_MISS = 8.0;   // px: a lone end this near another rail's way is a near miss

inline double way_distance(Point p, const Rail& r) {
    const Point a = end_of(r, 0), b = end_of(r, 1);
    const double ux = b.x - a.x, uy = b.y - a.y, len2 = ux * ux + uy * uy;
    double t = len2 > 0 ? ((p.x - a.x) * ux + (p.y - a.y) * uy) / len2 : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return std::hypot(p.x - a.x - ux * t, p.y - a.y - uy * t);
}

// The engine's rail point of end i (scale 0.25) or its look-out (0.75), truncated as crt_ftol does.
inline void engine_point(const Rail& r, int i, double scale, int& x, int& y) {
    x = static_cast<int>(r.x + ENDS[r.shape][i].x * 2 * HALF_X * scale);
    y = static_cast<int>(r.y + ENDS[r.shape][i].y * 2 * HALF_Y * scale);
}

inline bool engine_links(const Rail& a, int i, const Rail& b) {
    int lx, ly;
    engine_point(a, i, 0.75, lx, ly);
    for (int k = 0; k < 2; ++k) {
        if (height_of(b, k) != height_of(a, i)) continue;
        int px, py, bx, by, ax, ay;
        engine_point(b, k, 0.25, px, py);
        if (std::abs(px - lx) < 2 && std::abs(py - ly) < 2) return true;   // a's look-out finds b's point
        engine_point(b, k, 0.75, bx, by);
        engine_point(a, i, 0.25, ax, ay);
        if (std::abs(ax - bx) < 2 && std::abs(ay - by) < 2) return true;   // or b's finds a's
    }
    return false;
}

// The rails by their centres in square cells of BUCKET px (the review of 2026-11-12, finding 3: track_report
// asked every rail at every joint, O(rails^2) - 1.7 s for 16660 rails). What can matter at a joint p lies within
// 90 px of it: a rail ending there (its centre 43.4 px off at most), one the engine links within 2 px through
// either look-out (89 px: 1.5 of the half-diagonal and the point's 21.7), one whose way passes within NEAR_MISS
// (43.4 + 8) - so the 3 x 3 cells round p's cell hold all of them.
constexpr double BUCKET = 96;

struct RailGrid {
    std::unordered_map<long long, std::vector<int>> cells;
    static long long key(long long cx, long long cy) { return cx * 1000003LL + cy; }
    static long long cell(double v) { return static_cast<long long>(std::floor(v / BUCKET)); }
    explicit RailGrid(const std::vector<Rail>& rails) {
        for (size_t n = 0; n < rails.size(); ++n) cells[key(cell(rails[n].x), cell(rails[n].y))].push_back(static_cast<int>(n));
    }
    // the indices of the rails whose centres lie in the 3 x 3 cells round p, in their order in `rails`
    std::vector<int> around(Point p) const {
        std::vector<int> out;
        const long long cx = cell(p.x), cy = cell(p.y);
        for (long long dy = -1; dy <= 1; ++dy)
            for (long long dx = -1; dx <= 1; ++dx) {
                const auto found = cells.find(key(cx + dx, cy + dy));
                if (found != cells.end()) out.insert(out.end(), found->second.begin(), found->second.end());
            }
        std::sort(out.begin(), out.end());
        return out;
    }
};

// What the end `end` at height z of a rail at the joint p is (above), among `rails`; `self` is that
// rail's index in them (-1: a rail not among them yet - the Track tool's preview). `other` gets the
// lowest other height for Heights. `only`: the indices of the rails to look at (RailGrid::around), or all.
inline End end_state(const std::vector<Rail>& rails, Point p, Vec end, int z, int self, int* other = nullptr,
                     const std::vector<int>* only = nullptr) {
    bool any = false, same = false, opposite_other = false;
    int lowest = INT_MAX;
    const size_t count = only ? only->size() : rails.size();
    auto index = [&](size_t k) { return only ? static_cast<size_t>((*only)[k]) : k; };
    for (size_t k = 0; k < count; ++k) {
        const size_t n = index(k);
        if (static_cast<int>(n) == self) continue;
        const Rail& r = rails[n];
        for (int i = 0; i < 2; ++i) {
            const Point e = end_of(r, i);
            if (!close(e.x, p.x) || !close(e.y, p.y)) continue;
            any = true;
            const Vec v = ENDS[r.shape][i];
            const int h = height_of(r, i);
            if (v == end.opposite() && h == z) return End::Linked;
            if (v == end.opposite()) {
                opposite_other = true;
                if (h < lowest) lowest = h;
            }
            if (v == end && h == z) same = true;
        }
    }
    // the rail's end as the engine has it (or will have it: a preview's): only its centre, the end
    // vector and the point's height count for its point and look-out
    Rail mine = {p.x - end.x * HALF_X, p.y - end.y * HALF_Y, 0, z, z};
    int i_mine = 0;
    for (int s = 0; s < SHAPES; ++s)
        for (int i = 0; i < 2; ++i)
            if (ENDS[s][i] == end) {
                mine.shape = s;
                i_mine = i;
            }
    bool beside = false;
    for (size_t k = 0; k < count; ++k) {
        const size_t n = index(k);
        if (static_cast<int>(n) == self) continue;
        if (engine_links(mine, i_mine, rails[n])) return End::Linked;   // within the engine's 2 px
        if (!any && way_distance(p, rails[n]) <= NEAR_MISS) beside = true;
    }
    if (opposite_other) {
        if (other) *other = lowest;
        return End::Heights;
    }
    if (same) return End::Sharp;
    if (any) return End::DeadEnd;
    return beside ? End::Near : End::Loose;
}

// One place of a map's track to look at (track_report).
enum class Joint { Twice, Links, Heights, Sharp, DeadEnd, Near, Loose };

inline const char* joint_code(Joint j) {
    switch (j) {
    case Joint::Twice: return "twice";
    case Joint::Links: return "links";
    case Joint::Heights: return "heights";
    case Joint::Sharp: return "sharp";
    case Joint::DeadEnd: return "deadend";
    case Joint::Near: return "near";
    case Joint::Loose: return "loose";
    }
    return "?";
}

struct Problem {
    Joint kind;
    Point at;          // the joint (Twice: the rail's centre)
    Vec end = {0, 0};  // the rail ends there (Twice: none)
    int z = 0;         // the height of their point (Twice: of the rail's end 0)
    int value = 0;     // Twice: how many times; Links: the links; Heights: the lowest other height
    int rail = -1;     // a rail (its index in the input) with that end there, or the doubled one
    int shape = -1;    // Twice: the doubled rail's shape and the height of its end 1
    int z1 = 0;
};

// Every place of the track where the engine's network is not what the track looks like (map_tool.py
// track_report, which check-track prints): a rail laid twice (Twice - its points are only shared, no
// link added: harmless, but one of the two stays when the other is deleted), a rail point of more
// than MAX_LINKS links (Links - the engine drops the others, "Too many links"), and every rail end
// that is not Linked (end_state): Heights, Sharp, DeadEnd, Near and - with `loose` - Loose. The
// rails laid twice count once for the joints. In the order map_tool.py gives them: the doubled rails
// by their centre, then the joints by x, y, the ends of one joint by their end vector and height.
inline std::vector<Problem> track_report(const std::vector<Rail>& rails, bool loose = false) {
    struct Key {
        double x, y;
        int shape, z0, z1;
        bool operator<(const Key& o) const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            if (shape != o.shape) return shape < o.shape;
            if (z0 != o.z0) return z0 < o.z0;
            return z1 < o.z1;
        }
    };
    std::vector<Problem> out;
    std::vector<Rail> unique;
    std::vector<int> index;   // each unique rail's first index in `rails`
    {
        std::vector<std::pair<Key, int>> keys;
        for (size_t n = 0; n < rails.size(); ++n)
            keys.push_back({{round3(rails[n].x), round3(rails[n].y), rails[n].shape, rails[n].z0, rails[n].z1}, static_cast<int>(n)});
        std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t n = 0; n < keys.size();) {
            size_t m = n;
            while (m < keys.size() && !(keys[n].first < keys[m].first) && !(keys[m].first < keys[n].first)) ++m;
            const Key& k = keys[n].first;
            if (m - n > 1)
                out.push_back({Joint::Twice, {k.x, k.y}, {0, 0}, k.z0, static_cast<int>(m - n), keys[n].second, k.shape, k.z1});
            unique.push_back({k.x, k.y, k.shape, k.z0, k.z1});
            index.push_back(keys[n].second);
            n = m;
        }
    }
    struct At {
        double x, y;
        Vec end;
        int z, rail;
    };
    std::vector<At> ends;
    for (size_t n = 0; n < unique.size(); ++n)
        for (int i = 0; i < 2; ++i) {
            const Point e = end_of(unique[n], i);
            ends.push_back({round3(e.x), round3(e.y), ENDS[unique[n].shape][i], height_of(unique[n], i), static_cast<int>(n)});
        }
    std::sort(ends.begin(), ends.end(), [](const At& a, const At& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        if (a.end.x != b.end.x) return a.end.x < b.end.x;
        if (a.end.y != b.end.y) return a.end.y < b.end.y;
        if (a.z != b.z) return a.z < b.z;
        return a.rail < b.rail;
    });
    const RailGrid grid(unique);
    for (size_t n = 0; n < ends.size();) {   // a joint
        size_t m = n;
        while (m < ends.size() && ends[m].x == ends[n].x && ends[m].y == ends[n].y) ++m;
        const Point p = {ends[n].x, ends[n].y};
        std::vector<Rail> there;
        for (size_t k = n; k < m; ++k) there.push_back(unique[static_cast<size_t>(ends[k].rail)]);
        const std::vector<JointPoint> links = point_links(there, p);
        const std::vector<int> nearby = grid.around(p);
        for (size_t k = n; k < m;) {   // one point of the joint: its end vector and height
            size_t q = k;
            while (q < m && ends[q].end == ends[k].end && ends[q].z == ends[k].z) ++q;
            const At& a = ends[k];
            const int rail = index[static_cast<size_t>(a.rail)];
            for (const JointPoint& j : links)
                if (j.end == a.end && j.z == a.z && j.links > MAX_LINKS)
                    out.push_back({Joint::Links, p, a.end, a.z, j.links, rail});
            int other = 0;
            const End s = end_state(unique, p, a.end, a.z, a.rail, &other, &nearby);
            const Joint kind = s == End::Heights ? Joint::Heights : s == End::Sharp ? Joint::Sharp : s == End::DeadEnd ? Joint::DeadEnd
                               : s == End::Near ? Joint::Near : Joint::Loose;
            if (s != End::Linked && (s != End::Loose || loose)) out.push_back({kind, p, a.end, a.z, s == End::Heights ? other : 0, rail});
            k = q;
        }
        n = m;
    }
    return out;
}

using Cell = std::pair<int, int>;   // column, row

// Python's int(v) // 8: truncation toward zero, then a floor division.
inline int cell_of(double v) {
    const int t = static_cast<int>(v);
    return t >= 0 ? t / GRID_CELL : -((-t + GRID_CELL - 1) / GRID_CELL);
}

// The grid cells a track crosses and the one-cell buffer around them (track_cells).
inline void cells(const std::vector<Segment>& segments, int width, int height, std::set<Cell>& track,
                  std::set<Cell>& buffer) {
    track.clear();
    buffer.clear();
    for (const Segment& s : segments) {
        const double steps = std::fmax(std::fabs(s.b.x - s.a.x), std::fabs(s.b.y - s.a.y));
        for (int i = 0; i <= static_cast<int>(steps); ++i) {
            const double t = steps != 0 ? i / steps : 0.0;
            const int cx = cell_of(s.a.x + (s.b.x - s.a.x) * t), cy = cell_of(s.a.y + (s.b.y - s.a.y) * t);
            if (cx >= 0 && cx < width && cy >= 0 && cy < height) track.insert({cx, cy});
        }
    }
    for (const Cell& c : track)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const Cell n = {c.first + dx, c.second + dy};
                if (n.first >= 0 && n.first < width && n.second >= 0 && n.second < height && !track.count(n))
                    buffer.insert(n);
            }
}

// The height a buffer cell gets: half way toward the track (Python's floor division).
inline int ramp(int original, int level) {
    const int d = level - original;
    return original + (d >= 0 ? d / 2 : -((-d + 1) / 2));
}

}  // namespace rail
