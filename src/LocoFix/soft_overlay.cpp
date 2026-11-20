// Software pixels put on the picture without a lock of it (soft_overlay.h). Made from layer10_lock.cpp's way of
// putting the messages on the picture (fix 59), shared with the editor's overlay (fix 61); the pixels go up from a
// surface of their box's size (fix 60).
#include "soft_overlay.h"
#include "patch.h"
#include "perf.h"
#include <ddraw.h>
#include <algorithm>
#include <cstring>

namespace soft_overlay {

namespace {

constexpr uint16_t FILL[2] = {0x0000, 0xFFFF};

template <typename T> T& at(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }

IDirectDraw7* ddraw_of(IDirectDrawSurface7* s) {
    static const GUID IID_DD7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
    IUnknown* unknown = nullptr;
    if (FAILED(s->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return nullptr;
    IDirectDraw7* dd = nullptr;
    unknown->QueryInterface(IID_DD7, reinterpret_cast<void**>(&dd));
    unknown->Release();
    return dd;
}

void device_scene(uint8_t* g, int slot) {   // IDirect3DDevice7::BeginScene (5) / EndScene (6)
    void* device = at<void*>(g, 0xbf4);
    reinterpret_cast<HRESULT(__stdcall*)(void*)>((*reinterpret_cast<void***>(device))[slot])(device);
}

}  // namespace

bool Overlay::Watched::make(size_t bytes, uint16_t value) {
    if (bits && size == bytes) return true;
    if (bits) VirtualFree(bits, 0, MEM_RELEASE);
    bits = static_cast<uint16_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE));
    size = bits ? bytes : 0;
    if (!bits) return false;
    std::fill(bits, bits + bytes / 2, value);
    ResetWriteWatch(bits, size);
    return true;
}

void Overlay::Watched::written(std::vector<size_t>& pages, DWORD& page_size) {
    static std::vector<void*> addresses;
    addresses.resize(size / 4096 + 1);
    ULONG_PTR n = addresses.size();
    page_size = 4096;
    pages.clear();
    if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, bits, size, addresses.data(), &n, &page_size) != 0) return;
    for (ULONG_PTR i = 0; i < n; ++i) pages.push_back((static_cast<uint8_t*>(addresses[i]) - reinterpret_cast<uint8_t*>(bits)) / 2);
}

bool Overlay::make(int w, int h) {
    const size_t bytes = static_cast<size_t>(w) * h * 2;
    if (!pass_[0].make(bytes, FILL[0]) || !pass_[1].make(bytes, FILL[1])) return false;
    if (w_ != w || h_ != h) {
        w_ = w;
        h_ = h;
        set_.assign(static_cast<size_t>(w) * h, 0);
    }
    return true;
}

bool Overlay::collect() {
    const int w = w_, h = h_;
    left_ = w;
    top_ = h;
    right_ = bottom_ = -1;
    static std::vector<size_t> pages[2];
    rows_.assign(h, 0);
    for (int k = 0; k < 2; ++k) {
        DWORD page = 4096;
        pass_[k].written(pages[k], page);
        for (size_t first : pages[k]) {
            const size_t last = (std::min)(first + static_cast<size_t>(page / 2), static_cast<size_t>(w) * h) - 1;
            for (size_t y = first / w; y <= last / w; ++y) rows_[y] = 1;
        }
    }
    uint16_t* a = pass_[0].bits;
    uint16_t* b = pass_[1].bits;
    for (int y = 0; y < h; ++y) {
        if (!rows_[y]) continue;
        const size_t r = static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            const bool drawn = a[r + x] != FILL[0] || b[r + x] != FILL[1];
            set_[r + x] = drawn;
            if (!drawn) continue;
            if (a[r + x] == FILL[0]) a[r + x] = b[r + x];   // the first pass wrote FILL[0] itself
            left_ = (std::min)(left_, x);
            right_ = (std::max)(right_, x);
            top_ = (std::min)(top_, y);
            bottom_ = (std::max)(bottom_, y);
        }
    }
    return right_ >= left_;
}

bool Overlay::make_surface(void* ddraw, IDirectDrawSurface7* back, IDirectDrawSurface7*& s, int w, int h, DWORD caps) {
    auto* dd = static_cast<IDirectDraw7*>(ddraw);
    if (s) s->Release();
    s = nullptr;
    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddpfPixelFormat.dwSize = sizeof desc.ddpfPixelFormat;
    back->GetPixelFormat(&desc.ddpfPixelFormat);
    desc.ddsCaps.dwCaps = caps;
    const HRESULT hr = desc.ddpfPixelFormat.dwRGBBitCount == 16 ? dd->CreateSurface(&desc, &s, nullptr) : E_FAIL;
    if (FAILED(hr)) {
        s = nullptr;
        if (!failure_logged_) {
            failure_logged_ = true;
            patch::log("%s: no %dx%d surface (%08X) - it locks the picture as before", name_, w, h, static_cast<unsigned>(hr));
        }
    }
    return s != nullptr;
}

bool Overlay::ensure_surfaces(IDirectDrawSurface7* back) {
    if (cut_ && blocks_ && surface_w_ == w_ && surface_h_ == h_) {
        if (cut_->IsLost() == DDERR_SURFACELOST && FAILED(cut_->Restore())) return false;
        return true;
    }
    IDirectDraw7* dd = ddraw_of(back);
    if (!dd) return false;
    bool ok = make_surface(dd, back, cut_, w_, h_, DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY);
    if (ok && !blocks_) ok = SUCCEEDED(dd->CreateClipper(0, &blocks_, nullptr));
    dd->Release();
    surface_w_ = ok ? w_ : 0;
    surface_h_ = ok ? h_ : 0;
    return ok;
}

// A system-memory surface for the drawn pixels' box (w x h rounded up to 64 x 32; fix 60): a Blt from system memory
// has dgVoodoo upload the whole surface - a screen-sized one, as fix 59 had, cost 1.2 ms a Blt at 1920 x 1080 in the
// test rig, one of the box's size 0.02 ms. Up to 8 kept, the one used longest ago made anew. perf::off(60) (test
// commands 40053, 40056, 40057): one of the screen's size, as before.
IDirectDrawSurface7* Overlay::box_surface(IDirectDrawSurface7* back, int w, int h) {
    const bool whole = perf::off(60);
    w = whole ? w_ : (std::min)((w + 63) / 64 * 64, w_);
    h = whole ? h_ : (std::min)((h + 31) / 32 * 32, h_);
    ++uses_;
    for (Box& b : boxes_)
        if (b.w == w && b.h == h) {
            if (b.s->IsLost() == DDERR_SURFACELOST && FAILED(b.s->Restore())) return nullptr;
            b.used = uses_;
            return b.s;
        }
    IDirectDraw7* dd = ddraw_of(back);
    if (!dd) return nullptr;
    IDirectDrawSurface7* s = nullptr;
    make_surface(dd, back, s, w, h, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY);
    dd->Release();
    if (!s) return nullptr;
    if (boxes_.size() >= 8) {
        auto oldest = std::min_element(boxes_.begin(), boxes_.end(), [](const Box& a, const Box& b) { return a.used < b.used; });
        oldest->s->Release();
        boxes_.erase(oldest);
    }
    boxes_.push_back({w, h, s, uses_});
    return s;
}

bool Overlay::put(uint8_t* g, IDirectDrawSurface7* back) {    if (!ensure_surfaces(back)) return false;
    const int w = w_;
    const int bl = (std::max)(left_ - 1, 0), bt = (std::max)(top_ - 1, 0);
    const int br = right_ + 1, bb = bottom_ + 1;   // exclusive
    // the overlay: the pixels drawn, a key no one of them has elsewhere
    static uint8_t used[0x10000 / 8];
    std::memset(used, 0, sizeof used);
    const uint16_t* value = pass_[0].bits;
    for (int y = top_; y <= bottom_; ++y)
        for (int x = left_; x <= right_; ++x)
            if (set_[static_cast<size_t>(y) * w + x]) {
                const uint16_t v = value[static_cast<size_t>(y) * w + x];
                used[v >> 3] |= static_cast<uint8_t>(1 << (v & 7));
            }
    uint16_t key = 0xF81F;
    while (used[key >> 3] & (1 << (key & 7))) ++key;
    // the pixels drawn into a surface of their box's size, from its top left
    IDirectDrawSurface7* box = box_surface(back, right_ + 1 - left_, bottom_ + 1 - top_);
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (!box || FAILED(box->Lock(nullptr, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return false;
    for (int y = top_; y <= bottom_; ++y) {
        auto* row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(d.lpSurface) + (y - top_) * d.lPitch);
        for (int x = left_; x <= right_; ++x) {
            const size_t k = static_cast<size_t>(y) * w + x;
            row[x - left_] = set_[k] ? value[k] : key;
        }
    }
    box->Unlock(nullptr);
    // the blocks: a drawn pixel (x, y) marks (x - 1 .. x, y - 1 .. y); rows of runs, a run that goes on from
    // the row above with the same columns makes that rectangle taller
    static std::vector<RECT> rects;
    rects.clear();
    static std::vector<int> open;   // rects index of the runs of the row above
    static std::vector<int> next;
    open.clear();
    for (int y = bt; y < bb; ++y) {
        next.clear();
        size_t o = 0;
        for (int x = bl; x < br;) {
            auto marked = [&](int px) {
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        const int sx = px + dx, sy = y + dy;
                        if (sx >= left_ && sx <= right_ && sy >= top_ && sy <= bottom_ && set_[static_cast<size_t>(sy) * w + sx])
                            return true;
                    }
                return false;
            };
            if (!marked(x)) {
                ++x;
                continue;
            }
            int e = x;
            while (e < br && marked(e)) ++e;
            while (o < open.size() && rects[open[o]].right < e) ++o;   // runs of the row above, in order
            if (o < open.size() && rects[open[o]].left == x && rects[open[o]].right == e && rects[open[o]].bottom == y) {
                rects[open[o]].bottom = y + 1;
                next.push_back(open[o]);
            } else {
                rects.push_back({x, y, e, y + 1});
                next.push_back(static_cast<int>(rects.size()) - 1);
            }
            x = e;
        }
        open.swap(next);
    }
    static std::vector<uint8_t> region;
    region.assign(sizeof(RGNDATAHEADER) + rects.size() * sizeof(RECT), 0);
    auto* rgn = reinterpret_cast<RGNDATA*>(region.data());
    rgn->rdh.dwSize = sizeof(RGNDATAHEADER);
    rgn->rdh.iType = RDH_RECTANGLES;
    rgn->rdh.nCount = static_cast<DWORD>(rects.size());
    rgn->rdh.nRgnSize = static_cast<DWORD>(rects.size() * sizeof(RECT));
    rgn->rdh.rcBound = {bl, bt, br, bb};
    std::memcpy(rgn->Buffer, rects.data(), rects.size() * sizeof(RECT));
    DDCOLORKEY ck = {key, key};
    RECT area = {bl, bt, br, bb};
    RECT drawn = {left_, top_, right_ + 1, bottom_ + 1};
    RECT from = {0, 0, right_ + 1 - left_, bottom_ + 1 - top_};
    if (FAILED(box->SetColorKey(DDCKEY_SRCBLT, &ck)) || FAILED(blocks_->SetClipList(rgn, 0))) return false;
    device_scene(g, 6);
    bool ok = SUCCEEDED(cut_->Blt(&area, back, &area, DDBLT_WAIT, nullptr));
    if (ok && SUCCEEDED(back->SetClipper(blocks_))) {
        ok = SUCCEEDED(back->Blt(&area, cut_, &area, DDBLT_WAIT, nullptr));
        back->SetClipper(nullptr);
    } else {
        ok = false;
    }
    if (ok) ok = SUCCEEDED(back->Blt(&drawn, box, &from, DDBLT_KEYSRC | DDBLT_WAIT, nullptr));
    device_scene(g, 5);
    return ok;
}

void Overlay::write_into(uint16_t* bits, int pitch) const {
    const uint16_t* a = pass_[0].bits;
    for (int y = top_; y <= bottom_; ++y)
        for (int x = left_; x <= right_; ++x)
            if (set_[static_cast<size_t>(y) * w_ + x]) bits[static_cast<size_t>(y) * pitch + x] = a[static_cast<size_t>(y) * w_ + x];
}

void Overlay::clear() {
    uint16_t* a = pass_[0].bits;
    uint16_t* b = pass_[1].bits;
    for (int y = 0; y < h_ && y < static_cast<int>(rows_.size()); ++y)
        if (rows_[y]) {
            std::fill(a + static_cast<size_t>(y) * w_, a + static_cast<size_t>(y + 1) * w_, FILL[0]);
            std::fill(b + static_cast<size_t>(y) * w_, b + static_cast<size_t>(y + 1) * w_, FILL[1]);
            std::fill(set_.begin() + static_cast<size_t>(y) * w_, set_.begin() + static_cast<size_t>(y + 1) * w_, static_cast<uint8_t>(0));
        }
    for (Watched& p : pass_)   // the refill's own writes are no drawing
        ResetWriteWatch(p.bits, p.size);
}

}  // namespace soft_overlay
