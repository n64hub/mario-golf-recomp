// The iris wipe at its own shape in a wide window (RT64's Aspect Ratio Expand).
//
// Between screens (Course Select into a hole, a hole's end) the game closes and opens an iris in the shape of a
// character's head over the picture: one texture rectangle from x 8 to 312 and y 0 to 272, a small mask texture drawn
// many times its size (dsdx and dtdy equal, from about 1/12 to 2 texels a pixel), black around the shape and see-through
// inside it, its s and t moved and scaled each frame to grow or shrink the shape about the screen's middle. RT64
// stretches a rectangle as wide as its scissor across the whole window, as it does a screen's fill, so in a wide window
// the shape was drawn wider than it is, by the window's width over a 4:3 one's.
//
// nuGfxTaskStart sends each frame's display list, and title.cpp's hook there calls mg_iris_fix with it: it finds the
// mask's rectangle and sends it through a buffer that draws it kept at its aspect (G_EX_ASPECT_ADJUST) and from the
// window's left edge to its right one, its coordinates past the 4:3 frame's sides, under a scissor of the whole window,
// its s moved back by as many texels as the rectangle grew to the left. The mask's texels past the 4:3 frame are what
// the console would show there: black while the shape is inside the frame. The scissor and the aspect are set back
// after it. The buffers are this file's in RDRAM, above the 4 MB the game uses. With Aspect Ratio Original, or a window
// no wider than 4:3, nothing changes.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_DL_NOPUSH = 1, G_SC_NON_INTERLACE = 0;

// The mask's rectangle, in quarter pixels.
constexpr uint32_t MASK_LEFT = 8 * 4, MASK_RIGHT = 312 * 4, MASK_TOP = 0, MASK_BOTTOM = 272 * 4;
constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;

// Above ball.cpp's buffers. The renderer may read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80780000;
constexpr uint32_t BUFFER_SIZE = 0x100;
constexpr uint32_t BUFFER_COUNT = 16;
// The commands read from a list, at most.
constexpr uint32_t MAX_COMMANDS = 0x4000;

uint8_t* g_rdram;
uint32_t g_next_buffer = 0;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

struct Writer {
    uint32_t pos;
    void put(const GfxCommand* c, int count) {
        for (int i = 0; i < count; i++) {
            wr_w(pos, c[i].values.word0);
            wr_w(pos + 4, c[i].values.word1);
            pos += 8;
        }
    }
    void put(uint32_t w0, uint32_t w1) {
        wr_w(pos, w0);
        wr_w(pos + 4, w1);
        pos += 8;
    }
};

bool is_mask(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    if ((w0 >> 24) != G_TEXRECT || (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
        return false;
    }
    uint32_t d = rd_w(addr + 20);
    return ((w1 >> 12) & 0xFFF) == MASK_LEFT && (w1 & 0xFFF) == MASK_TOP && ((w0 >> 12) & 0xFFF) == MASK_RIGHT &&
           (w0 & 0xFFF) == MASK_BOTTOM && (d >> 16) == (d & 0xFFFF);
}

// The rectangle at addr (its three commands) replaced by a branch to a buffer that draws it at its aspect across the
// window, and branches back after it. left is the window's left edge in the 4:3 frame's pixels (negative): the
// rectangle's coordinates are the 4:3 frame's and run past it, since RT64 spans a rectangle aligned to the window's
// sides with its texture as wide as its coordinates say, not as the window is.
void widen(uint32_t addr, float left) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer };
    GfxCommand c[3];

    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t st = rd_w(addr + 12), d = rd_w(addr + 20);
    uint32_t tile = (w1 >> 24) & 7;
    int32_t s = int16_t(st >> 16), t = int16_t(st & 0xFFFF);
    uint32_t dsdx = d >> 16, dtdy = d & 0xFFFF;
    // The rectangle from the window's left edge to its right one, in the 4:3 frame's quarter pixels, and s at its left
    // edge: dsdx is s5.10 texels a pixel, s is s10.5.
    int32_t ulx = int32_t(std::floor(left)) * 4;
    int32_t lrx = SCREEN_WIDTH * 4 - ulx;
    int32_t s_left = s - (int32_t(MASK_LEFT) - ulx) * int32_t(dsdx) / 128;
    if (s_left < INT16_MIN) {
        s_left = INT16_MIN;
    }

    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
    out.put(c, 1);
    gEXPushScissor(c);
    out.put(c, 1);
    gEXSetScissor(c, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, SCREEN_HEIGHT);
    out.put(c, 2);
    gEXTextureRectangle(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, ulx & 0xFFFF, w1 & 0xFFF, lrx & 0xFFFF, w0 & 0xFFF, tile,
                        s_left & 0xFFFF, t & 0xFFFF, dsdx, dtdy);
    out.put(c, 3);
    gEXPopScissor(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    out.put(c, 1);
    out.put((G_DL << 24) | (G_DL_NOPUSH << 16), (addr + 24) & 0x1FFFFFFF);

    wr_w(addr, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

} // namespace

extern "C" void mg_iris_fix(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) != 0) {
        return;
    }
    for (uint32_t i = 0, addr = list; i < MAX_COMMANDS; i++, addr += 8) {
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_ENDDL) {
            break;
        }
        if (op != G_TEXRECT) {
            continue;
        }
        if (is_mask(addr)) {
            // Read only here, once a list holds the mask.
            float scale = mg_wide_scale();
            if (scale > 1.0f) {
                widen(addr, float(SCREEN_WIDTH) / 2.0f * (1.0f - scale));
            }
            break;
        }
        addr += 16;
        i += 2;
    }
}
