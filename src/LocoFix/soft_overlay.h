// Software pixels put on the picture without a lock of it (soft_overlay.cpp; engine.md "Locks under dgVoodoo").
//
// The engine's software sprites draw into the locked back buffer; under dgVoodoo every such lock after Direct3D
// drawing waits for the GPU and reads the picture back. Sprites that only write their own pixels (no blending)
// can instead be drawn twice into two screen-sized memories of ours, one filled with 0x0000 and one with 0xFFFF:
// a pixel was drawn where either pass changed it, its value the first pass's. Those pixels are then put on the
// picture as dgVoodoo's unlock would: the 2 x 2 blocks up-left of every pixel drawn cut to 16 bits by the GPU (a
// Blt through a 16-bit texture and back through a clip list of the blocks), then the pixels with a colour-keyed
// Blt. Used for the messages of layer 10 (layer10_lock.cpp, fix 59) and the editor's own overlay
// (editor_edit.cpp, fix 61).
#pragma once
#include <cstdint>
#include <vector>
#include <windows.h>

struct IDirectDrawSurface7;
struct IDirectDrawClipper;

namespace soft_overlay {

class Overlay {
public:
    explicit Overlay(const char* name) : name_(name) {}
    // The two passes' memories for a w x h picture (every value its fill but where a pass wrote); false: none.
    bool make(int w, int h);
    uint16_t* pass(int k) { return pass_[k].bits; }
    int width() const { return w_; }
    // After the passes: the pixels either pass wrote (their values in pass 0). True when any.
    bool collect();
    // Those pixels put on `back` as dgVoodoo's unlock would put them (the device's scene ended around the Blts);
    // false: not (all) on the picture - write_into a lock of it instead.
    bool put(uint8_t* g, IDirectDrawSurface7* back);
    // Those pixels written into a lock of the picture (pitch in pixels), as the engine would have drawn them.
    void write_into(uint16_t* bits, int pitch) const;
    // The rows written filled again for the next drawing.
    void clear();

private:
    struct Watched {   // screen-sized 16-bit memory, every value `fill` but where a pass wrote; writes watched
        uint16_t* bits = nullptr;
        size_t size = 0;   // bytes
        bool make(size_t bytes, uint16_t value);
        void written(std::vector<size_t>& pages, DWORD& page_size);   // offsets in pixels; the watch reset
    };
    bool ensure_surfaces(IDirectDrawSurface7* back);
    bool make_surface(void* dd, IDirectDrawSurface7* back, IDirectDrawSurface7*& s, int w, int h, DWORD caps);
    IDirectDrawSurface7* box_surface(IDirectDrawSurface7* back, int w, int h);

    const char* name_;
    Watched pass_[2];
    int w_ = 0, h_ = 0;
    std::vector<uint8_t> set_;   // w x h: drawn
    std::vector<uint8_t> rows_;  // h: written by a pass
    int left_ = 0, top_ = 0, right_ = -1, bottom_ = -1;   // the drawn pixels' box
    struct Box {   // system memory: the pixels drawn, the size of their box (rounded up)
        int w, h;
        IDirectDrawSurface7* s;
        unsigned used;
    };
    std::vector<Box> boxes_;
    unsigned uses_ = 0;
    IDirectDrawSurface7* cut_ = nullptr;       // a 16-bit texture: the picture's colour cut as a lock's copy cuts it
    IDirectDrawClipper* blocks_ = nullptr;
    int surface_w_ = 0, surface_h_ = 0;
    bool failure_logged_ = false;
};

}  // namespace soft_overlay
