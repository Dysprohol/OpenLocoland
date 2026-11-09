// A car of a train laid onto itself or onto the car in front of it (BUGS.md F24).
//
// The cars keep no path. Each frame Train_MoveCars puts every following car's front end on the
// rear end of the car in front, and Engine_UpdateRear (0x40a310) lays the car's rear end
// Weapon.length (48 px) behind its front from the front's point (front.dot, the point behind the
// front end), the link back from it and the old rear point. A rear RailPos counts from the point on
// the car's front side along a link toward the back; when the rear end lies on the front's own link,
// rear.dot is that link's far point and rear.distance = link length - front.distance + 48, which may
// be more than the link - the rear end then goes on past the end of the track along the link's line.
// The branches (d = front.distance, L = the length of the link back):
//     one    d > 48: the formula above.
//     two    rear.dot is front.dot: rear.distance = 48 - d along rear.link.
//     three  the front did not cross a point: the link from front.dot to rear.dot (RailDot_FindLink;
//            none: "ENGINE n: 0 rail 1" and RailDot_LinkTowards(front.dot, the front link's direction
//            - 0x80)); L + d > 48: on that link (the front's own: the formula above); else
//            rear.distance = 48 - L - d beyond rear.dot along rear.link.
//     four, five  the front crossed a point: the same from the point it left (zmdots5 / zmdots6).
// Two cases lay the rear end on the car's own side:
//   - front.dot is a dead end (a point of one link: a buffer stop, the end of a factory's spur): branch
//     two lays the rear 48 - d forward along the one link, and branch three with L + d <= 48 lays it
//     48 - L - d from the far point toward the front - a car coupled behind a car that hangs past a
//     buffer stop comes out turned round with its rear under the car in front, a car created on a
//     buffer stop collapses onto its own front;
//   - rear.link leads from rear.dot back to front.dot while the rear end belongs beyond rear.dot
//     (branch three, four or five), so "beyond" goes back toward the car and the rear end is reflected
//     onto it. At a dead end the one link is that way; elsewhere Engine_PutOnRails leaves rear.link so
//     (each end goes on the link nearest to it, counted from the nearer point), so a car made by a
//     script Create on open track, or next to a junction, can come out so too. A factory standing on
//     its spur's dead end (ten of the campaign's) released every car 34 px long and drawn 12 px forward;
//     two carts made 48 px apart on a main line came out 12 px apart, the second on the first.
// The game itself notices only the jump when such a car moves again and its rear end is laid anew:
// "zm-error" (in Russian "the cars collapse") in error.log.
//
// The fix: the original runs first, and only a result that is folded - the rear end nearer the car in
// front than the front end, or the ends under 40 px apart with the front end farther from the dead end
// or turning point than the rear end - is corrected, in those two cases only: in the first, for a car
// with a car in front, the rear end goes past the dead end on the line of its link (branch one's
// formula, what branch three gives when L + d > 48; a car alone is left as it is), in the second the same distance beyond rear.dot on the link that goes on from it the
// straightest (RailDot_LinkTowards with the direction from front.dot to rear.dot), and at a dead end
// past it on the line of the last link. Every other result is left exactly as the original made it:
// the missions save single cars against a buffer stop, laid on its link and drawn where they were
// placed (l06, l12, l15), and a train facing one with its front end at it (l12's neutral train).
// Each time a car comes into one of the cases LocoFix.log gets one "train fold:" line.
//
// TRAIN_FOLD_CHECK (a test build: cl -DTRAIN_FOLD_CHECK, TRAIN_FOLD_OFF leaves the fix out): after
// Train_MoveCars and Engine_AttachToTrain every car whose ends are under 40 px apart, or whose rear end
// is nearer the car in front than its front end, is logged ("train fold check: FOLDED ..."), with
// every coupling, contact and car of another train lying on a car.
#include "patch.h"

#include <cmath>
#include <cstdio>
#include <set>
#ifdef TRAIN_FOLD_CHECK
#include <map>
#endif

namespace {

constexpr uintptr_t ENGINE_UPDATE_REAR = 0x40a310;    // Engine method (previous front RailPos*)
constexpr uintptr_t RAIL_DOT_LINK_TOWARDS = 0x433f50;  // RailDot method (direction): the link nearest to it

// GameObject / Vid / Weapon / Engine / RailPos / RailDot / RailLink (re/types.txt)
constexpr size_t O_VID = 0x1c, O_X = 0x30, O_Y = 0x34;
constexpr size_t V_NVID = 0x04, V_WEAPON = 0x404, W_LENGTH = 0x08;
constexpr size_t E_PREV = 0xa4, E_FRONT = 0xc8, E_REAR = 0xd8;
constexpr size_t POS_DOT = 0x0, POS_DISTANCE = 0x4, POS_LINK = 0xc;
constexpr size_t DOT_LINK_COUNT = 0x18, DOT_LINKS = 0x1c, DOT_LINK_SIZE = 0x14, DOT_X = 0xe0, DOT_Y = 0xe4;
constexpr size_t LINK_DOT = 0x0, LINK_LENGTH = 0x4, LINK_BACK = 0x8, LINK_DIRECTION = 0x10;

template <typename T> T& at(void* p, size_t off) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off); }

uint8_t* link_at(uint8_t* dot, int k) { return dot + DOT_LINKS + k * DOT_LINK_SIZE; }

struct Pt {
    float x = 0, y = 0;
};

// The place of one end, interpolated along its link as Engine_UpdatePosition does (a distance over
// the length goes on past the point).
bool end_point(uint8_t* car, size_t pos, Pt& p) {
    uint8_t* d = at<uint8_t*>(car, pos + POS_DOT);
    const int k = at<int>(car, pos + POS_LINK);
    if (!d || k < 0 || k >= at<int>(d, DOT_LINK_COUNT)) return false;
    uint8_t* o = at<uint8_t*>(link_at(d, k), LINK_DOT);
    const int length = at<int>(link_at(d, k), LINK_LENGTH);
    if (!o || length <= 0) return false;
    const float t = static_cast<float>(at<int>(car, pos + POS_DISTANCE)) / length;
    p.x = at<int>(d, DOT_X) + (at<int>(o, DOT_X) - at<int>(d, DOT_X)) * t;
    p.y = at<int>(d, DOT_Y) + (at<int>(o, DOT_Y) - at<int>(d, DOT_Y)) * t;
    return true;
}

float rail_distance(Pt a, Pt b) {   // y counts 1.5 times, as in RailLink.length
    const float dx = a.x - b.x, dy = (a.y - b.y) * 1.5f;
    return std::sqrt(dx * dx + dy * dy);
}

int nvid_of(uint8_t* car) {
    uint8_t* v = at<uint8_t*>(car, O_VID);
    return v ? at<int>(v, V_NVID) : -1;
}

using UpdateRear_t = void(__fastcall*)(uint8_t* car, void* edx, uint8_t* previous);
using LinkTowards_t = int(__fastcall*)(uint8_t* dot, void* edx, int direction);
UpdateRear_t g_update_rear = nullptr;

std::set<uint8_t*> g_laid_past;   // cars in the corrected case, so that each one is logged once

// Whether Engine_UpdateRear will lay the rear end from a front whose point is a dead end: branch two
// (rear.dot is front.dot, so rear.link is the one link) or branch three (FindLink or LinkTowards can
// only give the one link). For a car with a car in front - its front end is that car's rear end - the
// rear end then belongs past that point on the line of the link, what branch three gives when link
// length + front.distance > 48 and nothing else does. A car alone or the first of its train is left as
// the original lays it: the missions save single cars against a buffer stop with both ends laid on
// its link (l06, l12, l15) and a train facing one (l12), drawn where they were placed. The weapon
// length as the engine converts it (crt_ftol truncates) goes to `whole`.
bool front_at_dead_end(uint8_t* car, uint8_t* previous, int& whole) {
    uint8_t* vid = at<uint8_t*>(car, O_VID);
    uint8_t* front = at<uint8_t*>(car, E_FRONT + POS_DOT);
    if (!vid || !at<uint8_t*>(vid, V_WEAPON) || !front || !previous || !at<uint8_t*>(car, E_PREV)) return false;
    const float length = at<float>(at<uint8_t*>(vid, V_WEAPON), W_LENGTH);
    if (length < static_cast<float>(at<int>(car, E_FRONT + POS_DISTANCE))) return false;   // branch one
    if (at<int>(front, DOT_LINK_COUNT) != 1 || at<int>(car, E_FRONT + POS_LINK) != 0) return false;
    if (!at<uint8_t*>(link_at(front, 0), LINK_DOT)) return false;
    if (at<uint8_t*>(car, E_REAR + POS_DOT) != front && at<uint8_t*>(previous, POS_DOT) != front) return false;   // branch four, five
    whole = static_cast<int>(length);
    return true;
}

// After the original: the rear end beyond its point B (rear.dot) along rear.link, where that link
// leads back to the front's point A and the front end lies on A's far side - the rear end was meant
// to be rear.distance beyond B and came out that far on the way back to A. rear.link comes so from
// Engine_PutOnRails (each end on the link nearest to it, from the nearer point) or, at a dead end,
// from its one link.
bool reflected(uint8_t* car) {
    uint8_t* b = at<uint8_t*>(car, E_REAR + POS_DOT);
    uint8_t* a = at<uint8_t*>(car, E_FRONT + POS_DOT);
    const int k = at<int>(car, E_REAR + POS_LINK);
    if (!a || !b || a == b || k < 0 || k >= at<int>(b, DOT_LINK_COUNT)) return false;
    if (at<uint8_t*>(link_at(b, k), LINK_DOT) != a || at<int>(car, E_REAR + POS_DISTANCE) <= 0) return false;
    const int front_link = at<int>(car, E_FRONT + POS_LINK);
    if (front_link < 0 || front_link >= at<int>(a, DOT_LINK_COUNT)) return false;
    return at<uint8_t*>(link_at(a, front_link), LINK_DOT) != b;   // the front leads away from B
}

// Whether the original laid the car folded, `end` being the dead end or the point its rear turned back
// at: the rear end nearer the middle of the car in front than the front end is, or the two ends under
// 40 px apart (48 along the rails; the sharpest turn a train drives keeps them over 44) with the front
// end farther from `end` than the rear end. A car that came out whole is left as it is.
bool laid_folded(uint8_t* car, uint8_t* end) {
    Pt f, r;
    if (!end_point(car, E_FRONT, f) || !end_point(car, E_REAR, r)) return false;
    if (uint8_t* prev = at<uint8_t*>(car, E_PREV)) {
        const Pt c{at<float>(prev, O_X), at<float>(prev, O_Y)};
        if (rail_distance(r, c) < rail_distance(f, c)) return true;
    }
    const Pt e{static_cast<float>(at<int>(end, DOT_X)), static_cast<float>(at<int>(end, DOT_Y))};
    return rail_distance(f, r) < 40.f && rail_distance(f, e) > rail_distance(r, e);
}

void log_once(uint8_t* car, const char* what, uint8_t* dot, int length, int past, const char* where) {
    if (g_laid_past.insert(car).second)
        patch::log("train fold: car %d at %.0f,%.0f - %s %d,%d (a link of %d px): its rear end laid %d px %s",
                   nvid_of(car), at<float>(car, O_X), at<float>(car, O_Y), what, at<int>(dot, DOT_X), at<int>(dot, DOT_Y), length,
                   past, where);
}

void __fastcall UpdateRear_fixed(uint8_t* car, void* edx, uint8_t* previous) {
    int whole = 0;
    const bool front_at_end = front_at_dead_end(car, previous, whole);
    g_update_rear(car, edx, previous);
    if (front_at_end) {
        // the rear on the front's link past front.dot: rear.dot the link's far point, the way back
        // along it, 48 - front distance + link length from there (branch one's formula)
        uint8_t* front = at<uint8_t*>(car, E_FRONT + POS_DOT);
        uint8_t* link = link_at(front, 0);
        uint8_t* far_dot = at<uint8_t*>(link, LINK_DOT);
        const int length = at<int>(link, LINK_LENGTH);
        const int distance = whole - at<int>(car, E_FRONT + POS_DISTANCE) + length;
        if (at<uint8_t*>(car, E_REAR + POS_DOT) == far_dot && at<int>(car, E_REAR + POS_LINK) == at<int>(link, LINK_BACK) &&
            at<int>(car, E_REAR + POS_DISTANCE) == distance) {
            g_laid_past.erase(car);   // branch three laid it so itself
            return;
        }
        if (!laid_folded(car, front)) {
            g_laid_past.erase(car);
            return;
        }
        at<uint8_t*>(car, E_REAR + POS_DOT) = far_dot;
        at<int>(car, E_REAR + POS_LINK) = at<int>(link, LINK_BACK);
        at<int>(car, E_REAR + POS_DISTANCE) = distance;
        log_once(car, "its front at the dead end", front, length, distance - length, "past it");
        return;
    }
    if (reflected(car) && laid_folded(car, at<uint8_t*>(car, E_REAR + POS_DOT))) {
        uint8_t* b = at<uint8_t*>(car, E_REAR + POS_DOT);
        uint8_t* back = link_at(b, at<int>(car, E_REAR + POS_LINK));   // B to A
        const int past = at<int>(car, E_REAR + POS_DISTANCE);
        // the link of B that goes on from A to B the straightest (RailDot_LinkTowards, the direction
        // of A to B: the way back from B reversed)
        uint8_t* a = at<uint8_t*>(back, LINK_DOT);
        const int ab = (at<uint8_t>(link_at(a, at<int>(back, LINK_BACK)), LINK_DIRECTION)) & 0xff;
        const int k = reinterpret_cast<LinkTowards_t>(RAIL_DOT_LINK_TOWARDS)(b, nullptr, ab);
        if (k >= 0 && k < at<int>(b, DOT_LINK_COUNT) && at<uint8_t*>(link_at(b, k), LINK_DOT) != a) {
            at<int>(car, E_REAR + POS_LINK) = k;   // rear.distance beyond B on it
            log_once(car, "its rear end turned back at", b, at<int>(link_at(b, k), LINK_LENGTH), past, "beyond it on the way on");
            return;
        }
        // a dead end: rear.distance past B on the link from A to B, beyond its length
        at<uint8_t*>(car, E_REAR + POS_DOT) = a;
        at<int>(car, E_REAR + POS_LINK) = at<int>(back, LINK_BACK);
        at<int>(car, E_REAR + POS_DISTANCE) = at<int>(back, LINK_LENGTH) + past;
        log_once(car, "its rear end beyond the dead end", b, at<int>(back, LINK_LENGTH), past, "past it");
        return;
    }
    g_laid_past.erase(car);
}

#ifdef TRAIN_FOLD_CHECK
constexpr uintptr_t TRAIN_MOVE_CARS = 0x40adf0;   // Engine method (front RailPos*, speed, direction)
constexpr uintptr_t ENGINE_ATTACH = 0x409270;     // Engine method (other): 0 when coupled
constexpr size_t E_FLAGS = 0xa0, E_NEXT = 0xa8;

using MoveCars_t = void(__fastcall*)(uint8_t* car, void* edx, void* front, float speed, int direction);
using Attach_t = int(__fastcall*)(uint8_t* car, void* edx, uint8_t* other);
MoveCars_t g_move_cars = nullptr;
Attach_t g_attach = nullptr;

std::string pos_text(uint8_t* car, size_t pos) {
    uint8_t* d = at<uint8_t*>(car, pos + POS_DOT);
    const int k = at<int>(car, pos + POS_LINK);
    char text[160];
    if (!d) return "none";
    const int count = at<int>(d, DOT_LINK_COUNT);
    if (k < 0 || k >= count) {
        std::snprintf(text, sizeof text, "%d,%d links %d link %d (bad) dist %d", at<int>(d, DOT_X), at<int>(d, DOT_Y), count, k,
                      at<int>(car, pos + POS_DISTANCE));
        return text;
    }
    uint8_t* o = at<uint8_t*>(link_at(d, k), LINK_DOT);
    std::snprintf(text, sizeof text, "%d,%d links %d -> %d,%d len %d dist %d", at<int>(d, DOT_X), at<int>(d, DOT_Y), count,
                  o ? at<int>(o, DOT_X) : 0, o ? at<int>(o, DOT_Y) : 0, at<int>(link_at(d, k), LINK_LENGTH),
                  at<int>(car, pos + POS_DISTANCE));
    return text;
}

bool folded(uint8_t* car, float& span, bool& backwards) {
    Pt f, r;
    span = 0;
    backwards = false;
    if (!end_point(car, E_FRONT, f) || !end_point(car, E_REAR, r)) return false;
    span = rail_distance(f, r);
    if (uint8_t* prev = at<uint8_t*>(car, E_PREV)) {
        const Pt c{at<float>(prev, O_X), at<float>(prev, O_Y)};
        backwards = rail_distance(r, c) < rail_distance(f, c);
    }
    return span < 40.f || backwards;
}

std::map<uint8_t*, bool> g_state;
std::map<uint8_t*, uint8_t*> g_overlap;   // car -> the car of another train logged on top of it
constexpr size_t DOT_OCCUPANT = 0xdc;

uint8_t* first_of(uint8_t* c) {
    while (at<uint8_t*>(c, E_PREV)) c = at<uint8_t*>(c, E_PREV);
    return c;
}
int train_size(uint8_t* c) {
    int n = 0;
    for (c = first_of(c); c; c = at<uint8_t*>(c, E_NEXT)) ++n;
    return n;
}

// A car of another train standing on the car's points with its middle under 24 px from this one's.
void check_overlap(uint8_t* c) {
    uint8_t* found = nullptr;
    for (size_t pos : {E_FRONT, E_REAR}) {
        uint8_t* d = at<uint8_t*>(c, pos + POS_DOT);
        uint8_t* o = d ? at<uint8_t*>(d, DOT_OCCUPANT) : nullptr;
        if (!o || o == c || first_of(o) == first_of(c)) continue;
        if (rail_distance({at<float>(c, O_X), at<float>(c, O_Y)}, {at<float>(o, O_X), at<float>(o, O_Y)}) < 24.f) found = o;
    }
    const auto it = g_overlap.find(c);
    uint8_t* was = it == g_overlap.end() ? nullptr : it->second;
    if (found == was) return;
    g_overlap[c] = found;
    if (found)
        patch::log("train fold check: OVERLAP car %d at %.0f,%.0f (train of %d, first %d at %.0f,%.0f) and car %d at %.0f,%.0f "
                   "(train of %d, first %d at %.0f,%.0f)", nvid_of(c), at<float>(c, O_X), at<float>(c, O_Y), train_size(c),
                   nvid_of(first_of(c)), at<float>(first_of(c), O_X), at<float>(first_of(c), O_Y), nvid_of(found),
                   at<float>(found, O_X), at<float>(found, O_Y), train_size(found), nvid_of(first_of(found)),
                   at<float>(first_of(found), O_X), at<float>(first_of(found), O_Y));
    else
        patch::log("train fold check: no overlap any more for car %d at %.0f,%.0f", nvid_of(c), at<float>(c, O_X), at<float>(c, O_Y));
}

using FindTouching_t = uint8_t*(__fastcall*)(uint8_t* car, void* edx, int* side);
FindTouching_t g_find_touching = nullptr;
std::map<uint8_t*, std::pair<uint8_t*, int>> g_contact;
uint8_t* __fastcall FindTouching_checked(uint8_t* car, void* edx, int* side) {
    uint8_t* other = g_find_touching(car, edx, side);
    const std::pair<uint8_t*, int> now{other, other ? *side : 0};
    auto& was = g_contact[car];
    if (now != was && other)
        patch::log("train fold check: contact %d: car %d at %.0f,%.0f (first of %d, back %u; front %s) touches car %d at %.0f,%.0f "
                   "(train of %d, first %d; back %u; front %s; rear %s)", *side, nvid_of(car), at<float>(car, O_X), at<float>(car, O_Y),
                   train_size(car), at<uint32_t>(car, E_FLAGS) & 1, pos_text(car, E_FRONT).c_str(), nvid_of(other),
                   at<float>(other, O_X), at<float>(other, O_Y), train_size(other), nvid_of(first_of(other)),
                   at<uint32_t>(other, E_FLAGS) & 1, pos_text(other, E_FRONT).c_str(), pos_text(other, E_REAR).c_str());
    was = now;
    return other;
}

void check_train(uint8_t* first, const char* where) {
    for (uint8_t* c = first; c; c = at<uint8_t*>(c, E_NEXT)) {
        check_overlap(c);
        float span;
        bool backwards;
        const bool now = folded(c, span, backwards);
        const auto it = g_state.find(c);
        if (now == (it != g_state.end() && it->second)) continue;
        g_state[c] = now;
        uint8_t* prev = at<uint8_t*>(c, E_PREV);
        patch::log("train fold check: %s car %d at %.0f,%.0f (span %.1f%s, back %u) after %s; front %s; rear %s; car in front %d at %.0f,%.0f",
                   now ? "FOLDED" : "unfolded", nvid_of(c), at<float>(c, O_X), at<float>(c, O_Y), span,
                   backwards ? ", rear toward the car in front" : "", at<uint32_t>(c, E_FLAGS) & 1, where,
                   pos_text(c, E_FRONT).c_str(), pos_text(c, E_REAR).c_str(), prev ? nvid_of(prev) : -1,
                   prev ? at<float>(prev, O_X) : 0.f, prev ? at<float>(prev, O_Y) : 0.f);
    }
}

void __fastcall MoveCars_checked(uint8_t* car, void* edx, void* front, float speed, int direction) {
    g_move_cars(car, edx, front, speed, direction);
    check_train(car, "Train_MoveCars");
}

int __fastcall Attach_checked(uint8_t* car, void* edx, uint8_t* other) {
    const int result = g_attach(car, edx, other);
    if (other)
        patch::log("train fold check: attach car %d at %.0f,%.0f to car %d at %.0f,%.0f: %d, now a train of %d; front %s; rear %s", nvid_of(car),
                   at<float>(car, O_X), at<float>(car, O_Y), nvid_of(other), at<float>(other, O_X), at<float>(other, O_Y), result,
                   train_size(car), pos_text(car, E_FRONT).c_str(), pos_text(car, E_REAR).c_str());
    if (result == 0) {
        uint8_t* first = car;
        while (at<uint8_t*>(first, E_PREV)) first = at<uint8_t*>(first, E_PREV);
        check_train(first, "Engine_AttachToTrain");
    }
    return result;
}
#endif

}  // namespace

void install_train_fold() {
#ifndef TRAIN_FOLD_OFF
    const uint8_t update_rear[] = {0x83, 0xEC, 0x08, 0x53, 0x55, 0x56};   // sub esp,8 / push ebx / ebp / esi
    g_update_rear = reinterpret_cast<UpdateRear_t>(patch::hook(ENGINE_UPDATE_REAR, update_rear, sizeof update_rear,
                                                                reinterpret_cast<void*>(&UpdateRear_fixed), "Engine_UpdateRear"));
#endif
#ifdef TRAIN_FOLD_CHECK
    const uint8_t move_cars[] = {0x51, 0x53, 0x55, 0x56, 0x57};   // push ecx / ebx / ebp / esi / edi
    const uint8_t attach[] = {0x83, 0xEC, 0x20, 0x56, 0x8B, 0x74, 0x24, 0x28};   // sub esp,0x20 / push esi / mov esi,[esp+0x28]
    g_move_cars = reinterpret_cast<MoveCars_t>(
        patch::hook(TRAIN_MOVE_CARS, move_cars, sizeof move_cars, reinterpret_cast<void*>(&MoveCars_checked), "Train_MoveCars"));
    g_attach = reinterpret_cast<Attach_t>(
        patch::hook(ENGINE_ATTACH, attach, sizeof attach, reinterpret_cast<void*>(&Attach_checked), "Engine_AttachToTrain"));
    const uint8_t touching[] = {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x7C, 0x24, 0x0C};   // push esi / mov esi,ecx / push edi / mov edi,[esp+0xc]
    g_find_touching = reinterpret_cast<FindTouching_t>(patch::hook(0x409d00, touching, sizeof touching,
                                                                   reinterpret_cast<void*>(&FindTouching_checked), "Engine_FindTouchingCar"));
    patch::log("train fold check: on%s", g_update_rear ? "" : " (the fix off)");
#endif
}
