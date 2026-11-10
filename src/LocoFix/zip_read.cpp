// The zip reader (zip_read.h), moved here from editor_exchange.cpp unchanged.
#include "zip_read.h"

#include <algorithm>
#include <iterator>

namespace zipfile {

// ---- CRC-32 ----

uint32_t crc32(const std::string& d) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xffffffffu;
    for (unsigned char b : d) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

// ---- the codes of deflate ----

extern const int LENGTH_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
extern const int LENGTH_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
extern const int DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                  6145, 8193, 12289, 16385, 24577};
extern const int DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// ---- inflate (RFC 1951): stored, fixed and dynamic blocks ----

struct BitReader {
    const unsigned char* p;
    size_t n, at = 0;
    uint32_t bits = 0;
    int count = 0;
    bool bad = false;
    int get(int need) {
        while (count < need) {
            if (at >= n) {
                bad = true;
                return 0;
            }
            bits |= static_cast<uint32_t>(p[at++]) << count;
            count += 8;
        }
        const int v = static_cast<int>(bits & ((1u << need) - 1));
        bits >>= need;
        count -= need;
        return v;
    }
};

struct Huffman {
    short counts[16] = {};
    std::vector<short> symbols;
    void build(const short* lengths, int n) {
        std::fill(std::begin(counts), std::end(counts), 0);
        for (int i = 0; i < n; ++i) ++counts[lengths[i]];
        counts[0] = 0;
        short offsets[16] = {};
        for (int i = 1; i < 16; ++i) offsets[i] = static_cast<short>(offsets[i - 1] + counts[i - 1]);
        symbols.assign(n, 0);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offsets[lengths[i]]++] = static_cast<short>(i);
    }
    int decode(BitReader& r) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= r.get(1);
            const int count = counts[len];
            if (code - count < first) return symbols[index + (code - first)];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
            if (r.bad) return -1;
        }
        return -1;
    }
};

bool inflate_codes(BitReader& r, std::string& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        const int sym = lit.decode(r);
        if (sym < 0 || r.bad) return false;
        if (sym < 256) {
            out += static_cast<char>(sym);
        } else if (sym == 256) {
            return true;
        } else {
            const int k = sym - 257;
            if (k >= 29) return false;
            const int len = LENGTH_BASE[k] + r.get(LENGTH_EXTRA[k]);
            const int ds = dist.decode(r);
            if (ds < 0 || ds >= 30) return false;
            const size_t d = static_cast<size_t>(DIST_BASE[ds] + r.get(DIST_EXTRA[ds]));
            if (d > out.size() || r.bad) return false;
            for (int i = 0; i < len; ++i) out += out[out.size() - d];
        }
    }
}

bool inflate(const std::string& in, std::string& out, size_t expected) {
    BitReader r{reinterpret_cast<const unsigned char*>(in.data()), in.size()};
    out.clear();
    out.reserve(expected);
    for (int last = 0; !last;) {
        last = r.get(1);
        const int type = r.get(2);
        if (r.bad) return false;
        if (type == 0) {   // stored
            r.bits = 0;
            r.count = 0;
            if (r.at + 4 > r.n) return false;
            const size_t len = r.p[r.at] | (r.p[r.at + 1] << 8);
            r.at += 4;
            if (r.at + len > r.n) return false;
            out.append(reinterpret_cast<const char*>(r.p + r.at), len);
            r.at += len;
        } else if (type == 1) {   // fixed
            static Huffman lit, dist;
            if (lit.symbols.empty()) {
                short l[288], d[30];
                for (int i = 0; i < 288; ++i) l[i] = static_cast<short>(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
                for (int i = 0; i < 30; ++i) d[i] = 5;
                lit.build(l, 288);
                dist.build(d, 30);
            }
            if (!inflate_codes(r, out, lit, dist)) return false;
        } else if (type == 2) {   // dynamic
            const int nlen = r.get(5) + 257, ndist = r.get(5) + 1, ncode = r.get(4) + 4;
            static const int ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            short lengths[320] = {};
            for (int i = 0; i < ncode; ++i) lengths[ORDER[i]] = static_cast<short>(r.get(3));
            Huffman code;
            code.build(lengths, 19);
            short all[320] = {};
            for (int i = 0; i < nlen + ndist;) {
                const int sym = code.decode(r);
                if (sym < 0 || r.bad) return false;
                if (sym < 16) {
                    all[i++] = static_cast<short>(sym);
                } else {
                    short v = 0;
                    int repeat;
                    if (sym == 16) {
                        if (!i) return false;
                        v = all[i - 1];
                        repeat = 3 + r.get(2);
                    } else if (sym == 17) {
                        repeat = 3 + r.get(3);
                    } else {
                        repeat = 11 + r.get(7);
                    }
                    if (i + repeat > nlen + ndist) return false;
                    while (repeat--) all[i++] = v;
                }
            }
            Huffman lit, dist;
            lit.build(all, nlen);
            dist.build(all + nlen, ndist);
            if (!inflate_codes(r, out, lit, dist)) return false;
        } else {
            return false;
        }
    }
    return true;
}

// ---- zip ----

uint32_t u16(const std::string& d, size_t at) { return at + 2 <= d.size() ? static_cast<unsigned char>(d[at]) | static_cast<unsigned char>(d[at + 1]) << 8 : 0; }
uint32_t u32(const std::string& d, size_t at) { return at + 4 <= d.size() ? u16(d, at) | u16(d, at + 2) << 16 : 0; }

bool read_zip(const std::string& z, std::vector<Entry>& entries, std::string& error) {
    size_t end = std::string::npos;
    for (size_t i = z.size() >= 22 ? z.size() - 22 : 0; i + 22 <= z.size() && i + 0x10000 + 22 >= z.size(); --i) {
        if (u32(z, i) == 0x06054b50) {
            end = i;
            break;
        }
        if (!i) break;
    }
    if (end == std::string::npos) {
        error = "not a zip file";
        return false;
    }
    const uint32_t count = u16(z, end + 10);
    size_t at = u32(z, end + 16);
    for (uint32_t k = 0; k < count; ++k) {
        if (u32(z, at) != 0x02014b50) {
            error = "a damaged zip file";
            return false;
        }
        const uint32_t method = u16(z, at + 10), crc = u32(z, at + 16), packed = u32(z, at + 20), size = u32(z, at + 24);
        const uint32_t name_len = u16(z, at + 28), extra = u16(z, at + 30), comment = u16(z, at + 32), local = u32(z, at + 42);
        Entry e;
        e.name = z.substr(at + 46, name_len);
        at += 46 + name_len + extra + comment;
        if (e.name.empty() || e.name.back() == '/' || e.name.back() == '\\') continue;   // a folder
        if (u32(z, local) != 0x04034b50) {
            error = "a damaged zip file";
            return false;
        }
        const size_t data = local + 30 + u16(z, local + 26) + u16(z, local + 28);
        if (data + packed > z.size()) {
            error = "a damaged zip file";
            return false;
        }
        const std::string raw = z.substr(data, packed);
        if (method == 0) {
            e.data = raw;
        } else if (method != 8 || !inflate(raw, e.data, size)) {
            error = "a file of the zip cannot be unpacked: " + e.name;
            return false;
        }
        if (e.data.size() != size || crc32(e.data) != crc) {
            error = "a file of the zip is damaged: " + e.name;
            return false;
        }
        entries.push_back(e);
    }
    return true;
}

}  // namespace zipfile
