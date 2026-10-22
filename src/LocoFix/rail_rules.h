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
#include <climits>
#include <cmath>
#include <cstdlib>
#include <set>
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
};

struct Plan {
    Error error = Error::None;
    int step = -1;   // the step it failed at (-1: links, or none)
    double turn = 0; // degrees, for Sharp
    std::vector<Segment> segments;
};

// The rails of a track along `points` next to `rails` (map_tool.py plan_track). `shapes` (one
// per step, or empty) picks the shape by hand; `z` is the height of the track's rail points (its
// rails' z + ALTITUDE): it joins only the rails whose points are at that height, ANY_Z all.
inline Plan plan(const std::vector<Rail>& rails, const std::vector<Point>& points,
                 const std::vector<int>& shapes = {}, int z = ANY_Z) {
    Plan p;
    if (points.size() < 2) return p;
    std::vector<Vec> entry;
    for (const Vec& e : ends_at(rails, points[0], z)) {
        const Vec o = e.opposite();
        if (!contains(entry, o)) entry.push_back(o);
    }
    bool have_entry = !entry.empty();
    bool have_previous = false;
    double previous = 0;
    Vec second = {0, 0};
    for (size_t n = 0; n + 1 < points.size(); ++n) {
        const Point a = points[n], b = points[n + 1];
        std::vector<Candidate> cs = candidates(a, b);
        if (cs.empty()) {
            p.error = Error::Step;
            p.step = static_cast<int>(n);
            return p;
        }
        if (have_entry) {
            std::vector<Candidate> joined;
            for (const Candidate& c : cs)
                if (contains(entry, c.first)) joined.push_back(c);
            if (joined.empty()) {
                p.error = Error::Join;
                p.step = static_cast<int>(n);
                return p;
            }
            cs = joined;
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
            if (std::fabs(turn) > 45.0) {
                p.error = Error::Sharp;
                p.step = static_cast<int>(n);
                p.turn = turn;
                return p;
            }
        }
        previous = h;
        have_previous = true;
        for (const Rail& r : rails)
            if (close(r.x, c.x) && close(r.y, c.y) && r.shape == c.shape &&
                (z == ANY_Z || (r.z0 == z && r.z1 == z))) {
                p.error = Error::Duplicate;
                p.step = static_cast<int>(n);
                return p;
            }
        const Rail made = {c.x, c.y, c.shape};
        Point e0 = end_of(made, 0), e1 = end_of(made, 1);
        e0 = {round3(e0.x), round3(e0.y)};
        e1 = {round3(e1.x), round3(e1.y)};
        p.segments.push_back({a, b, c.x, c.y, c.shape, e0, e1});
    }
    const std::vector<Vec> meeting = ends_at(rails, points.back(), z);
    if (!meeting.empty() && !contains(meeting, second.opposite())) {
        p.error = Error::Last;
        p.step = static_cast<int>(points.size()) - 2;
        p.segments.clear();
        return p;
    }
    // no rail point may get more links than the engine allows: at every joint of the track, with
    // the rails already there (ANY_Z: the rails at any height as if at one)
    const int level = z == ANY_Z ? 0 : z;
    std::vector<Rail> laid;
    for (const Rail& r : rails)
        laid.push_back({r.x, r.y, r.shape, z == ANY_Z ? level : r.z0, z == ANY_Z ? level : r.z1});
    for (const Segment& s : p.segments) laid.push_back({s.x, s.y, s.shape, level, level});
    for (const Segment& s : p.segments)
        for (const Point q : {s.end0, s.end1})
            for (const JointPoint& j : point_links(laid, q))
                if (j.z == level && j.links > MAX_LINKS) {
                    p.error = Error::Links;
                    p.step = -1;
                    p.segments.clear();
                    return p;
                }
    return p;
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
