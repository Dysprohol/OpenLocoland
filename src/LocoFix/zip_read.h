// The zip reader shared by the map exchange (editor_exchange.cpp: File > Import map) and the launcher's
// update check (update_check.cpp: a one-folder Setup packed as a zip): CRC-32, inflate (RFC 1951:
// stored, fixed and dynamic blocks) and a whole zip read in memory (stored or deflated entries, each
// checked by its size and CRC). Moved here from editor_exchange.cpp unchanged.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zipfile {

struct Entry {
    std::string name, data;
};

uint32_t crc32(const std::string& d);

// the length and distance codes of deflate (RFC 1951 3.2.5), which the exchange's deflate uses too
extern const int LENGTH_BASE[29];
extern const int LENGTH_EXTRA[29];
extern const int DIST_BASE[30];
extern const int DIST_EXTRA[30];

// Little-endian numbers of `d` at `at`, 0 past its end.
uint32_t u16(const std::string& d, size_t at);
uint32_t u32(const std::string& d, size_t at);

// The raw deflate stream `in` unpacked into `out` (`expected`: the size to reserve).
bool inflate(const std::string& in, std::string& out, size_t expected);

// Every file of the zip `z` (folders left out), in the order of its central directory; false with
// `error` ("not a zip file", "a damaged zip file", "a file of the zip ... ") when it cannot be read.
bool read_zip(const std::string& z, std::vector<Entry>& entries, std::string& error);

}  // namespace zipfile
