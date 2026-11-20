// Test host for the launcher's version rule (src/LocoFix/update_version.h): which version is newer and
// which release of GitHub's list is offered. tools\test_release_versions.py builds and runs it; it prints
// a line per case that fails and exits with 1 when any does.
#include "../src/LocoFix/update_version.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failed = 0, g_run = 0;

void expect(bool ok, const std::string& what) {
    ++g_run;
    if (!ok) {
        ++g_failed;
        std::printf("FAILED: %s\n", what.c_str());
    }
}

int sign(int n) { return n < 0 ? -1 : n > 0 ? 1 : 0; }

const char* word(int s) { return s < 0 ? "older than" : s > 0 ? "newer than" : "the same as"; }

// a is `expected` (-1 older, 0 the same, 1 newer) than b, and b the other way round
void order(const char* a, const char* b, int expected) {
    const int got = sign(update_version::compare(a, b)), back = sign(update_version::compare(b, a));
    expect(got == expected && back == -expected,
           std::string(a) + " should be " + word(expected) + " " + b + " (got " + word(got) + ", back " + word(back) + ")");
}

void parsed(const char* v, bool valid, const char* pre = "", int pre_number = 0) {
    const update_version::Version p = update_version::parse(v);
    expect(p.valid == valid && (!valid || (p.pre == pre && p.pre_number == pre_number)),
           std::string(v) + (valid ? " should be a version, pre-release '" + std::string(pre) + "' " + std::to_string(pre_number)
                                   : " should be no version") +
               " (got " + (p.valid ? "a version, pre-release '" + p.pre + "' " + std::to_string(p.pre_number) : "no version") + ")");
}

// The launcher's decision as checked() makes it: "newer" (offered with Download) or "current".
const char* decision(const std::string& mine, const std::string& found) {
    const update_version::Version m = update_version::parse(mine), f = update_version::parse(found);
    if (!m.valid) return "dev";
    return f.valid && update_version::compare(f, m) > 0 ? "newer" : "current";
}

void decides(const char* mine, const char* found, const char* expected) {
    const std::string got = decision(mine, found);
    expect(got == expected, std::string("this build ") + mine + ", GitHub " + found + ": " + expected + " expected, got " + got);
}

std::string release(const char* tag, bool prerelease, bool draft = false) {
    // as GitHub writes one (shortened): the author and an asset have html_urls and ids of their own
    return std::string("{\"html_url\":\"https://github.com/o/r/releases/tag/") + tag +
           "\",\"author\":{\"login\":\"x\",\"html_url\":\"https://github.com/x\"},\"tag_name\":\"" + tag +
           "\",\"name\":\"OpenLocoland " + tag + " {draft} [x]\",\"draft\":" + (draft ? "true" : "false") +
           ",\"prerelease\":" + (prerelease ? "true" : "false") +
           ",\"assets\":[{\"name\":\"OpenLocoland-" + tag + "-Setup.exe\",\"uploader\":{\"login\":\"x\"},\"size\":1,"
           "\"browser_download_url\":\"https://github.com/o/r/releases/download/" + tag + "/OpenLocoland-" + tag +
           "-Setup.exe\"}],\"body\":\"Notes with \\\"tag_name\\\": \\\"v9.9.9\\\" and } ] { in them\"}";
}

// GitHub's list (newest first) offers `expected` ("" for none)
void offers(const std::vector<std::string>& list, const char* expected, const char* what) {
    std::string json = "[\n";
    for (size_t i = 0; i < list.size(); ++i) json += (i ? ",\n" : "") + list[i];
    json += "\n]";
    const std::vector<std::string> objects = update_version::json_array_objects(json);
    const int k = update_version::newest_release(objects);
    const std::string got =
        k < 0 ? "" : update_version::json_string(objects[k], update_version::json_value(objects[k], "tag_name", 0));
    expect(objects.size() == list.size() && got == expected,
           std::string(what) + ": " + (*expected ? expected : "none") + " expected, got " + (got.empty() ? "none" : got) +
               " (" + std::to_string(objects.size()) + " of " + std::to_string(list.size()) + " releases read)");
}

}  // namespace

int main() {
    // what is a version and what is not
    parsed("v0.2.12", true);
    parsed("0.2.12", true);
    parsed("v0.2.12-3-gabc1234", true);
    parsed("v0.2.12-dirty", true);
    parsed("v0.2.12-3-gabc1234-dirty", true);
    parsed("v0.2.13-beta.1", true, "beta", 1);
    parsed("v0.2.13-Beta.1", true, "beta", 1);
    parsed("v0.2.13-beta1", true, "beta", 1);
    parsed("v0.2.13-beta", true, "beta", 0);
    parsed("v0.2.13-rc.2", true, "rc", 2);
    parsed("v0.2.13-beta.1-2-gabc1234", true, "beta", 1);
    parsed("v0.2.13-beta.1-2-gabc1234-dirty", true, "beta", 1);
    parsed("v0.2.13-beta.1-dirty", true, "beta", 1);
    parsed("dev", false);
    parsed("", false);
    parsed("v", false);
    parsed("5476e33", false);    // git describe of a commit without a tag: a hash
    parsed("1234567", false);    // ... that happens to be all digits
    parsed("v0.2.13x", false);
    parsed("v0.2.13-beta.1x", false);
    parsed("v0.2.", false);
    parsed("v1234567.0", false);

    // the numbers
    order("v0.1.10", "v0.1.9", 1);
    order("v0.2.12", "v0.2.11", 1);
    order("v1.0.0", "v0.99.99", 1);
    order("v0.2", "v0.2.0", 0);
    order("v0.2.12", "v0.2.12", 0);
    // a build after a tag counts as the tag
    order("v0.2.12-3-gabc1234", "v0.2.12", 0);
    order("v0.2.12-dirty", "v0.2.12", 0);
    order("v0.2.13-beta.1-2-gabc1234", "v0.2.13-beta.1", 0);
    // pre-releases: after the release before, before their own release
    order("v0.2.13-beta.1", "v0.2.12", 1);
    order("v0.2.13-beta.1", "v0.2.12-5-gabc1234", 1);
    order("v0.2.13-beta.1", "v0.2.13", -1);
    order("v0.2.13-rc.1", "v0.2.13", -1);
    order("v0.2.13-beta.1-2-gabc1234", "v0.2.13", -1);
    order("v0.2.13-beta.2", "v0.2.13-beta.1", 1);
    order("v0.2.13-beta.10", "v0.2.13-beta.9", 1);
    order("v0.2.13-beta", "v0.2.13-beta.1", -1);
    order("v0.2.13-rc.1", "v0.2.13-beta.3", 1);
    order("v0.2.13-alpha.5", "v0.2.13-beta.1", -1);
    order("v0.2.14-beta.1", "v0.2.13", 1);
    // no version: never newer than a version
    order("dev", "v0.2.12", -1);
    order("dev", "5476e33", 0);

    // the launcher's line: the latest (the box off) or the list's newest (the box on) against this build
    decides("v0.2.12", "v0.2.12", "current");                       // a stable, the box off: latest is v0.2.12
    decides("v0.2.12", "v0.2.13-beta.1", "newer");                  // the box on: the list holds the beta
    decides("v0.2.12-4-gabc1234", "v0.2.13-beta.1", "newer");
    decides("v0.2.13-beta.1", "v0.2.12", "current");                // the beta, the box off: never an older stable
    decides("v0.2.13-beta.1", "v0.2.13-beta.1", "current");         // the beta, the box on
    decides("v0.2.13-beta.1", "v0.2.13", "newer");                  // the release after the beta: either setting
    decides("v0.2.13-beta.1-2-gabc1234", "v0.2.13-beta.1", "current");
    decides("v0.2.13-beta.1-2-gabc1234", "v0.2.13", "newer");
    decides("v0.2.13-beta.1", "v0.2.13-beta.2", "newer");
    decides("v0.2.13", "v0.2.13-beta.2", "current");
    decides("v0.2.13", "5476e33", "current");                       // a tag that is no version is never offered
    decides("dev", "v0.2.13", "dev");

    // GitHub's list, newest first
    offers({release("v0.2.13-beta.1", true), release("v0.2.12", false), release("v0.2.11", false)}, "v0.2.13-beta.1",
           "a beta over the latest");
    offers({release("v0.2.13", false), release("v0.2.13-beta.1", true), release("v0.2.12", false)}, "v0.2.13",
           "the release after its beta");
    offers({release("v0.2.14-beta.1", true, true), release("v0.2.13-beta.1", true), release("v0.2.12", false)},
           "v0.2.13-beta.1", "a draft left out");
    offers({release("v0.2.11.1", false),release("v0.2.13-beta.2", true), release("v0.2.13-beta.1", true)}, "v0.2.13-beta.2",
           "an older version made later");
    offers({release("nightly", true), release("v0.2.12", false)}, "v0.2.12", "a tag that is no version left out");
    offers({release("nightly", true)}, "nightly", "only tags that are no versions: the first");
    offers({release("v0.3.0-beta.1", true, true)}, "", "only a draft");
    offers({}, "", "no release");
    {
        // the answer cut off at its bound: the whole objects before the cut are read
        const std::string json = "[" + release("v0.2.13-beta.1", true) + "," + release("v0.2.14", false).substr(0, 60);
        const std::vector<std::string> objects = update_version::json_array_objects(json);
        expect(objects.size() == 1 && update_version::newest_release(objects) == 0, "an answer cut off: one release read");
        expect(update_version::json_array_objects(release("v0.2.12", false)).empty(), "one release (the latest) is no list");
        expect(update_version::json_true(release("v0.2.13-beta.1", true), "prerelease") &&
                   !update_version::json_true(release("v0.2.12", false), "prerelease"),
               "a release's prerelease mark");
    }

    std::printf("%d of %d checks passed\n", g_run - g_failed, g_run);
    return g_failed ? 1 : 0;
}
