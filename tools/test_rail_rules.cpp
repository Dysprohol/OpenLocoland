// Test host for the rail rules of the in-game editor (src/LocoFix/rail_rules.h).
//
// tools\test_rail_rules.py builds this and compares its answers with tools/map_tool.py. Each
// input line is one case:
//
//     C <ax> <ay> <bx> <by>                         rail_candidates(a, b)
//     P <n> {<x> <y> <shape> <z0> <z1>}*n <m> {<x> <y>}*m <k> {<shape>}*k <z>
//                                                   plan_track(rails, points, shapes or None, z)
//     T <width> <height> <n> {<ax> <ay> <bx> <by>}*n
//                                                   track_cells(segments, width, height)
//     R <x> <y> <z> <direction>                     rail_of(x, y, z, direction)
//     J <n> {<x> <y> <shape> <z0> <z1>}*n <px> <py>  rail_point_links(rails, point)
//
// (z "any" = None) and gets one output line: the candidates "x y shape fx fy sx sy" separated by
// ";", the plan "<code> <step> x y shape e0x e0y e1x e1y;...", the cells "cx,cy ...|cx,cy ..."
// (sorted), the rail "x y shape z0 z1", the points "ex ey z links;..." (sorted).
#define _CRT_SECURE_NO_WARNINGS
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
            in >> z;
            const rail::Plan plan = rail::plan(rails, points, shapes, z == "any" ? rail::ANY_Z : std::stoi(z));
            out = std::string(rail::error_code(plan.error)) + " " + std::to_string(plan.step);
            if (plan.error == rail::Error::None) {
                for (const rail::Segment& s : plan.segments)
                    out += " " + num(s.x) + " " + num(s.y) + " " + std::to_string(s.shape) + " " + num(s.end0.x) +
                           " " + num(s.end0.y) + " " + num(s.end1.x) + " " + num(s.end1.y) + ";";
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
        }
        std::cout << out << "\n";
    }
    return 0;
}
