// Map exchange (editor_exchange.cpp): a map's folder packed into one .zip, and unpacked again.
#pragma once
#include <string>
#include <vector>

struct PackageFile {
    std::string name, data;
};

// A map package as read: the folder's name in it, the map's title (the script's "// title:", else
// the name in capitals), the map's ground image as packed, the files of the map's folder.
struct Package {
    std::string name, title, ground;
    std::vector<PackageFile> files;
};

// Packs the saved map `map` (maps\<name>\<name>.map) and its folder - not the high scores - into `zip`.
bool exchange_export(const std::string& map, const std::string& zip, std::string& error, int& files);
// Reads a package; false (with `error`) when it is not a map's.
bool exchange_read(const std::string& zip, Package& p, std::string& error);
// Unpacks it into maps\<name>\ (renamed from its own name when they differ; `overwrite`: the old
// files there go first). The map's path, or empty with `error`.
std::string exchange_install(const Package& p, const std::string& name, bool overwrite, std::string& error);
// Its ground is a campaign terrain of the game (or none): nothing of the picture is in it.
bool exchange_campaign_ground(const Package& p);
