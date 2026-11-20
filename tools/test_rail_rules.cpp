// Test host for the rail rules of the in-game editor (src/LocoFix/rail_rules.h).
//
// tools\test_rail_rules.py builds this and compares its answers with tools/map_tool.py. Each
// input line is one case:
//
//     C <ax> <ay> <bx> <by>                         rail_candidates(a, b)
//     P <n> {<x> <y> <shape> <z0> <z1>}*n <m> {<x> <y>}*m <k> {<shape>}*k <z> <through> <lay> <ramp>
//                                                   plan_track(rails, points, shapes or None, z, through, lay, ramp)
//     T <width> <height> <n> {<ax> <ay> <bx> <by>}*n
//                                                   track_cells(segments, width, height)
//     R <x> <y> <z> <direction>                     rail_of(x, y, z, direction)
//     J <n> {<x> <y> <shape> <z0> <z1>}*n <px> <py>  rail_point_links(rails, point)
//     Q <n> {<x> <y> <shape> <z0> <z1>}*n <loose>    track_report(rails, loose)
//     W <n> {<x> <y> <z> <direction> <nvid>}*n <loose>  network_report of rail objects laid in that order (rail_net.h)
//
// (z "any" = None) and gets one output line: the candidates "x y shape fx fy sx sy" separated by
// ";", the plan "<code> <step> x y shape e0x e0y e1x e1y;...", the cells "cx,cy ...|cx,cy ..."
// (sorted), the rail "x y shape z0 z1", the points "ex ey z links;..." (sorted), the report
// "kind x y ex ey z value;..." in its order ("twice x y shape z0 z1 count;" for a rail laid twice); W the
// same report, "|" and each rail's way on at end 0 and end 1 ("on|reverse|none on|reverse|none;").
#define _CRT_SECURE_NO_WARNINGS
#include "../src/LocoFix/rail_net.h"
#include "../src/LocoFix/rail_rules.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string num(double v) {
    char text[32];
    std::snprintf(text, sizeof text, "%.3f", v);
    return text;
}

std::vector<rail::Rail> read_rails(std::istringstream& in) {
    int n;
    in >> n;
    std::vector<rail::Rail> rails;
    for (int i = 0; i < n; ++i) {
        rail::Rail r;
        in >> r.x >> r.y >> r.shape >> r.z0 >> r.z1;
        rails.push_back(r);
    }
    return rails;
}

}  // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string kind;
        in >> kind;
        std::string out;
        if (kind == "C") {
            rail::Point a, b;
            in >> a.x >> a.y >> b.x >> b.y;
            for (const rail::Candidate& c : rail::candidates(a, b)) {
                if (!out.empty()) out += ";";
                out += num(c.x) + " " + num(c.y) + " " + std::to_string(c.shape) + " " + std::to_string(c.first.x) +
                       " " + std::to_string(c.first.y) + " " + std::to_string(c.second.x) + " " +
                       std::to_string(c.second.y);
            }
        } else if (kind == "P") {
            int m, k;
            const std::vector<rail::Rail> rails = read_rails(in);
            std::vector<rail::Point> points;
            std::vector<int> shapes;
            in >> m;
            for (int i = 0; i < m; ++i) {
                rail::Point p;
                in >> p.x >> p.y;
                points.push_back(p);
            }
            in >> k;
            for (int i = 0; i < k; ++i) {
                int s;
                in >> s;
                shapes.push_back(s);
            }
            std::string z;
            int through = 0, lay = 0, ramp = 0;
            in >> z >> through >> lay >> ramp;
            const rail::Plan plan = rail::plan(rails, points, shapes, z == "any" ? rail::ANY_Z : std::stoi(z), through != 0, lay != 0, ramp);
            auto height = [](int h) { return h == rail::ANY_Z ? std::string("any") : std::to_string(h); };
            out = std::string(rail::error_code(plan.error)) + " " + std::to_string(plan.step);
            if (plan.error == rail::Error::None) {
                for (const rail::Segment& s : plan.segments)
                    out += " " + num(s.x) + " " + num(s.y) + " " + std::to_string(s.shape) + " " + num(s.end0.x) +
                           " " + num(s.end0.y) + " " + num(s.end1.x) + " " + num(s.end1.y) + " " + height(s.z0) + " " +
                           height(s.z1) + ";";
            }
        } else if (kind == "T") {
            int width, height, n;
            in >> width >> height >> n;
            std::vector<rail::Segment> segments;
            for (int i = 0; i < n; ++i) {
                rail::Segment s = {};
                in >> s.a.x >> s.a.y >> s.b.x >> s.b.y;
                segments.push_back(s);
            }
            std::set<rail::Cell> track, buffer;
            rail::cells(segments, width, height, track, buffer);
            for (const rail::Cell& c : track) out += std::to_string(c.first) + "," + std::to_string(c.second) + " ";
            out += "|";
            for (const rail::Cell& c : buffer) out += std::to_string(c.first) + "," + std::to_string(c.second) + " ";
        } else if (kind == "R") {
            double x, y, z;
            int direction;
            in >> x >> y >> z >> direction;
            const rail::Rail r = rail::rail_of(x, y, z, direction);
            out = num(r.x) + " " + num(r.y) + " " + std::to_string(r.shape) + " " + std::to_string(r.z0) + " " +
                  std::to_string(r.z1);
        } else if (kind == "J") {
            const std::vector<rail::Rail> rails = read_rails(in);
            rail::Point p;
            in >> p.x >> p.y;
            std::vector<std::string> items;
            for (const rail::JointPoint& j : rail::point_links(rails, p))
                items.push_back(std::to_string(j.end.x) + " " + std::to_string(j.end.y) + " " + std::to_string(j.z) +
                                " " + std::to_string(j.links) + ";");
            std::sort(items.begin(), items.end());
            for (const std::string& i : items) out += i;
        } else if (kind == "Q" || kind == "W") {
            std::vector<rail::Rail> rails;
            std::string ways;
            std::vector<rail::Problem> report;
            int loose = 0;
            if (kind == "Q") {
                rails = read_rails(in);
                in >> loose;
                report = rail::track_report(rails, loose != 0);
            } else {   // rail objects in the map's order, laid on the network (rail_net.h)
                int n;
                in >> n;
                rail::Network net;
                std::vector<std::pair<int, int>> nodes;
                for (int i = 0; i < n; ++i) {
                    float x, y, z;
                    int direction, nvid;
                    in >> x >> y >> z >> direction >> nvid;
                    rail::RailVid v;
                    v.altitude = nvid == 600 || nvid == 601 ? 7.0f : 10.0f;
                    rails.push_back(rail::rail_of(x, y, static_cast<double>(z) + v.altitude - rail::ALTITUDE, direction));
                    nodes.push_back(net.lay(x, y, z, direction, v));
                }
                for (const auto& nd : nodes)
                    ways += std::string(rail::way_code(net.way(nd.first, nd.second))) + " " + rail::way_code(net.way(nd.second, nd.first)) + ";";
                in >> loose;
                for (const rail::Problem& p : rail::network_report(rails, net, nodes))
                    if (p.kind != rail::Joint::Loose || loose) report.push_back(p);
            }
            for (const rail::Problem& p : report) {
                out += std::string(rail::joint_code(p.kind)) + " " + num(p.at.x) + " " + num(p.at.y) + " ";
                if (p.kind == rail::Joint::Twice)
                    out += std::to_string(p.shape) + " " + std::to_string(p.z) + " " + std::to_string(p.z1) + " " + std::to_string(p.value);
                else
                    out += std::to_string(p.end.x) + " " + std::to_string(p.end.y) + " " + std::to_string(p.z) + " " + std::to_string(p.value);
                out += ";";
            }
            if (kind == "W") out += "|" + ways;
        }
        std::cout << out << "\n";
    }
    return 0;
}
