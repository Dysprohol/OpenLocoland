// The rail network as the engine builds it (Rail_Action ACT_CHANGE_DIRECTION, RailMap_AddDot,
// RailMap_FindDot, RailDot_Connect; DEVELOPERS.md "The rail network") - for the editor's marks of the
// track's joints (issue #24: a mark said "joined" where the trains found no way on).
//
// rail_rules.h answers by the look of the track: two rail ends at one place pointing opposite ways at
// one height are joined, and so is an end whose look-out lands within 2 px of another rail's point or
// whose point another rail's look-out finds. The engine is narrower than that:
//  - a rail links only through its own look-outs, when it is laid (loaded): a point of a rail laid
//    later is never found by the earlier rail's look-out - so where only one of the two look-outs
//    finds the other's point (rails off the joints of the lattice: an end in the middle of a curve,
//    two diagonals whose tips are a lattice step apart) the link is there or not by the order of the
//    rails in the map;
//  - a link made does not mean a way on: a train reverses where the next link turns 0x20 (45 degrees)
//    or more from the one it came on (RailDot_Search), and a look-out landing on the side of another
//    rail links into it at such an angle;
//  - a point takes 6 links; a 7th is dropped on the full side (RailDot_Connect: "Too many links").
// So the marks ask this network instead: the rails laid one by one in the map's order, as loading
// builds it (Run the level and a played map load the map), and at each rail end whether a train going
// along the rail toward that end finds a link on within 0x20 of its way. Plain C++ without the game or
// Windows; tools/map_tool.py rail_network is the same in Python (check-track), and tools/test_rail_rules.py
// compares the two.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "rail_rules.h"

namespace rail {

// Math_DirectionOfVector: the direction 0-255 (0x40 a quarter turn) of (dx, dy) by CORDIC steps.
inline int direction_of(int dx, int dy) {
    static const int ANGLES[7] = {64, 37, 19, 10, 5, 2, 1};   // the table at 0x47e094
    int acc = 0x80;
    if (dx < 0) {
        acc = 0x180;
        dx = -dx;
        dy = -dy;
    }
    for (int s = 0; s < 7; ++s) {
        int nx, ny;
        if (dy < 0) {
            nx = dx + -(dy >> s);
            ny = dy + (dx >> s);
            acc -= ANGLES[s];
        } else {
            nx = dx + (dy >> s);
            ny = dy + -(dx >> s);
            acc += ANGLES[s];
        }
        dx = nx;
        dy = ny;
    }
    return (acc >> 1) & 0xff;
}

// What a train going along a rail toward one of its ends finds there (RailDot_Search's turn rule).
enum class Way {
    Pass,   // a link on within 0x20 of its way: the track goes on
    Turn,   // links on, all turning 0x20 or more: it can only reverse there
    None,   // no link on: the end of the track for the trains
};

// The values of a rail VID Rail_Action reads: its size, the height of its points above the rail
// (altitude) and how a ramp lifts the point of end 0 / end 1 by the frame's group (the engine's two
// static tables, which its first rail fills with that rail's sizeZ: 0, sizeZ, 0, 0 and 0, 0, sizeZ, 0).
struct RailVid {
    float size_x = 71, size_y = 50, altitude = static_cast<float>(ALTITUDE);
    int dir_half_step = HALF_STEP, frames = FRAMES;
    float lift0[4] = {0, static_cast<float>(RAMP), 0, 0}, lift1[4] = {0, 0, static_cast<float>(RAMP), 0};
};

struct Network {
    struct Link {
        int dot;
        int direction;
    };
    struct Dot {
        int x, y, z;
        int count = 0;
        Link links[MAX_LINKS];
    };
    std::vector<Dot> dots;   // in the order RailMap_AddDot adds them (RailMap_FindDot takes the first)

    // RailMap_FindDot: the first point within 1 px in x and y (|d| < 2) at the same height, or -1
    int find(int x, int y, int z) const {
        int best = -1;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const auto found = cells_.find(key(x + dx, y + dy));
                if (found == cells_.end()) continue;
                for (int n : found->second)
                    if (dots[static_cast<size_t>(n)].z == z && (best < 0 || n < best)) best = n;
            }
        return best;
    }

    // RailMap_AddDot (refCount aside): the point found there or a new one; the place truncated to whole px
    int add_dot(float x, float y, float z) {
        const int ix = static_cast<int>(x), iy = static_cast<int>(y), iz = static_cast<int>(z);
        const int found = find(ix, iy, iz);
        if (found >= 0) return found;
        Dot d;
        d.x = ix;
        d.y = iy;
        d.z = iz;
        dots.push_back(d);
        const int n = static_cast<int>(dots.size()) - 1;
        cells_[key(ix, iy)].push_back(n);
        journal_.push_back(-1 - n);   // a new point
        return n;
    }

    // RailDot_Connect: both ways when a has no link to b yet, each side only while it has fewer than 6
    void connect(int a, int b) {
        if (a < 0 || b < 0) return;
        Dot& da = dots[static_cast<size_t>(a)];
        for (int k = 0; k < da.count; ++k)
            if (da.links[k].dot == b) return;
        Dot& db = dots[static_cast<size_t>(b)];
        const int dir = direction_of(db.x - da.x, db.y - da.y);
        if (da.count < MAX_LINKS) {
            da.links[da.count++] = {b, dir};
            journal_.push_back(a);
        }
        if (db.count < MAX_LINKS) {
            db.links[db.count++] = {a, (dir - 0x80) & 0xff};
            journal_.push_back(b);
        }
    }

    // Rail_Action ACT_CHANGE_DIRECTION of a rail object at x, y, z turned to `direction`: its two points
    // a quarter of the sprite from its centre toward its ends (z + the ramp's lift + altitude), each linked
    // to the point found at its look-out three quarters out, then to each other. Returns the points of
    // end 0 and end 1.
    std::pair<int, int> lay(float x, float y, float z, int direction, const RailVid& v) {
        const int frame = ((v.dir_half_step + direction) & 0xff) * v.frames >> 8;
        const int group = (frame / 12) & 3, shape = frame % 12;
        const Vec e0 = ENDS[shape][0], e1 = ENDS[shape][1];
        const double z0 = static_cast<double>(z) + v.lift0[group] + v.altitude, z1 = static_cast<double>(z) + v.altitude + v.lift1[group];
        const int n0 = add_dot(static_cast<float>(v.size_x * e0.x * 0.25 + x), static_cast<float>(v.size_y * e0.y * 0.25 + y), static_cast<float>(z0));
        const int n1 = add_dot(static_cast<float>(v.size_x * e1.x * 0.25 + x), static_cast<float>(v.size_y * e1.y * 0.25 + y), static_cast<float>(z1));
        connect(n0, find(static_cast<int>(v.size_x * e0.x * 0.75 + x), static_cast<int>(v.size_y * e0.y * 0.75 + y), static_cast<int>(z0)));
        connect(n1, find(static_cast<int>(v.size_x * e1.x * 0.75 + x), static_cast<int>(v.size_y * e1.y * 0.75 + y), static_cast<int>(z1)));
        connect(n1, n0);
        return {n0, n1};
    }

    // At the point d of a rail end, coming from the rail's other point f.
    Way way(int d, int f) const {
        if (d < 0 || f < 0 || d == f) return Way::None;
        const Dot& dd = dots[static_cast<size_t>(d)];
        const Dot& df = dots[static_cast<size_t>(f)];
        int arrival = -1;
        for (int k = 0; k < df.count; ++k)
            if (df.links[k].dot == d) arrival = df.links[k].direction;
        if (arrival < 0) arrival = direction_of(dd.x - df.x, dd.y - df.y);
        bool other = false;
        for (int k = 0; k < dd.count; ++k) {
            if (dd.links[k].dot == f) continue;
            other = true;
            const int a = (arrival - dd.links[k].direction) & 0xff, b = (dd.links[k].direction - arrival) & 0xff;
            if ((a < b ? a : b) < 0x20) return Way::Pass;
        }
        return other ? Way::Turn : Way::None;
    }

    // A mark to go back to (the Track tool's preview lays its rails on the map's network and takes them
    // away again): the points and links added since are removed.
    size_t mark() const { return journal_.size(); }
    void back_to(size_t mark) {
        while (journal_.size() > mark) {
            const int j = journal_.back();
            journal_.pop_back();
            if (j >= 0) {
                --dots[static_cast<size_t>(j)].count;
            } else {
                const int n = -1 - j;
                std::vector<int>& cell = cells_[key(dots[static_cast<size_t>(n)].x, dots[static_cast<size_t>(n)].y)];
                cell.pop_back();
                dots.pop_back();
            }
        }
    }

private:
    static long long key(int x, int y) { return (static_cast<long long>(x) << 32) ^ static_cast<unsigned>(y); }
    std::unordered_map<long long, std::vector<int>> cells_;
    std::vector<int> journal_;   // what was added: a link of point n (n), a new point n (-1 - n)
};

// Whether a lone rail end (no other rail ends at p) looks joined to another rail: its look-out lands within
// the engine's 2 px of another rail's point at its height (a diagonal ending in the middle of a curve), or it
// lies within NEAR_MISS of another rail's way. Rails the same as the end's own (laid twice) do not count.
inline bool reaches(const std::vector<Rail>& rails, Point p, Vec end, int z, int self) {
    Rail mine = {p.x - end.x * HALF_X, p.y - end.y * HALF_Y, 0, z, z};
    int i_mine = 0;
    for (int s = 0; s < SHAPES; ++s)
        for (int i = 0; i < 2; ++i)
            if (ENDS[s][i] == end) {
                mine.shape = s;
                i_mine = i;
            }
    int lx, ly;
    engine_point(mine, i_mine, 0.75, lx, ly);
    for (size_t n = 0; n < rails.size(); ++n) {
        const Rail& r = rails[n];
        if (static_cast<int>(n) == self) continue;
        if (self >= 0) {
            const Rail& o = rails[static_cast<size_t>(self)];
            if (r.shape == o.shape && close(r.x, o.x) && close(r.y, o.y) && r.z0 == o.z0 && r.z1 == o.z1) continue;
        }
        for (int k = 0; k < 2; ++k) {
            int px, py;
            engine_point(r, k, 0.25, px, py);
            if (height_of(r, k) == z && std::abs(px - lx) < 2 && std::abs(py - ly) < 2) return true;
        }
        if (way_distance(p, r) <= NEAR_MISS) return true;
    }
    return false;
}

// What a rail end is in the end (the marks, Check the track, the preview's dot): the network's way on
// decides whether it joins; the look of the track (end_state) only names what is wrong where it does not.
// `others` - another rail ends at the place (any height); `reached` - a lone end looks joined (reaches).
// An end the look took for joined that has no link on is "not joined" where other rails end, a near miss
// where it reaches another rail, else the end of a track (a rail laid half along past another's end).
inline End truthful(End look, Way way, bool others, bool reached) {
    if (way == Way::Pass) return End::Linked;
    if (way == Way::Turn) return look == End::Heights ? End::Heights : End::Sharp;
    switch (look) {   // no link on
    case End::Linked:
    case End::Sharp: return others ? End::DeadEnd : reached ? End::Near : End::Loose;
    default: return look;
    }
}

inline const char* way_code(Way w) { return w == Way::Pass ? "on" : w == Way::Turn ? "reverse" : "none"; }

inline End end_of_joint(Joint j) {
    switch (j) {
    case Joint::Heights: return End::Heights;
    case Joint::Sharp: return End::Sharp;
    case Joint::DeadEnd: return End::DeadEnd;
    case Joint::Near: return End::Near;
    default: return End::Loose;
    }
}
inline Joint joint_of_end(End e) {
    switch (e) {
    case End::Heights: return Joint::Heights;
    case End::Sharp: return Joint::Sharp;
    case End::DeadEnd: return Joint::DeadEnd;
    case End::Near: return Joint::Near;
    default: return Joint::Loose;
    }
}

// track_report with the network's answer (above): an end the network passes on is not listed, one it
// does not is listed with the look's reason - or, where the look took it for joined, as truthful() names it:
// not joined, a near miss, the end of a track, or trains turning round (links on, all too sharp). `nodes`:
// each rail's points of end 0 and end 1 in `net`. The order is track_report's.
inline std::vector<Problem> network_report(const std::vector<Rail>& rails, const Network& net,
                                           const std::vector<std::pair<int, int>>& nodes) {
    struct Key {
        double x, y;
        int ex, ey, z;
        bool operator<(const Key& o) const { return std::tie(x, y, ex, ey, z) < std::tie(o.x, o.y, o.ex, o.ey, o.z); }
    };
    struct At {
        Way way = Way::None;
        int rail = -1;
    };
    std::map<Key, At> ends;
    std::map<std::pair<double, double>, int> keys_at;   // how many end keys each place has
    for (size_t n = 0; n < rails.size(); ++n)
        for (int i = 0; i < 2; ++i) {
            const Point e = end_of(rails[n], i);
            const Vec v = ENDS[rails[n].shape][i];
            const Key k = {round3(e.x), round3(e.y), v.x, v.y, height_of(rails[n], i)};
            const int d = i ? nodes[n].second : nodes[n].first, f = i ? nodes[n].first : nodes[n].second;
            const Way w = net.way(d, f);
            auto found = ends.find(k);
            if (found == ends.end()) {
                ends[k] = {w, static_cast<int>(n)};
                ++keys_at[{k.x, k.y}];
            } else if (w == Way::Pass || (w == Way::Turn && found->second.way == Way::None)) {
                found->second.way = w;
            }
        }
    std::vector<Problem> out;
    std::set<Key> looked;
    for (Problem p : track_report(rails, true)) {
        if (p.kind == Joint::Twice || p.kind == Joint::Links) {
            out.push_back(p);
            continue;
        }
        const Key k = {round3(p.at.x), round3(p.at.y), p.end.x, p.end.y, p.z};
        looked.insert(k);
        const auto found = ends.find(k);
        const Way w = found == ends.end() ? Way::None : found->second.way;
        const End look = end_of_joint(p.kind);
        const bool reached = look == End::Sharp && w == Way::None && found != ends.end() && reaches(rails, p.at, p.end, p.z, found->second.rail);
        const End e = truthful(look, w, keys_at[{k.x, k.y}] > 1, reached);
        if (e == End::Linked) continue;
        if (e != End::Heights) p.value = 0;
        p.kind = joint_of_end(e);
        out.push_back(p);
    }
    for (const auto& [k, a] : ends) {   // ends the look took for joined
        if (looked.count(k) || a.way == Way::Pass) continue;
        const End e = truthful(End::Linked, a.way, keys_at[{k.x, k.y}] > 1, reaches(rails, {k.x, k.y}, {k.ex, k.ey}, k.z, a.rail));
        out.push_back({joint_of_end(e), {k.x, k.y}, {k.ex, k.ey}, k.z, 0, a.rail});
    }
    const auto first = std::find_if(out.begin(), out.end(), [](const Problem& p) { return p.kind != Joint::Twice; });
    std::stable_sort(first, out.end(), [](const Problem& a, const Problem& b) {
        const int ra = a.kind == Joint::Links ? 0 : 1, rb = b.kind == Joint::Links ? 0 : 1;
        return std::tie(a.at.x, a.at.y, a.end.x, a.end.y, a.z, ra) < std::tie(b.at.x, b.at.y, b.end.x, b.end.y, b.z, rb);
    });
    return out;
}

}  // namespace rail
