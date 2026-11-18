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
// mask's rectangle and sends it through a buffer that draws it as RT64's extended texture rectangle stretched to the
// picture (G_EX_ASPECT_STRETCH), from the 4:3 frame's left edge to its right one, its texture step across multiplied
// by as much as the stretch widens it and its s moved so the shape's middle stays at the screen's: the shape keeps its
// own aspect about the middle, and its black spans the picture as RT64 stretches a screen's fill across it. The game's
// own scissor, the 3D's (8 to 312), clips it where the wide picture's course ends, so nothing of the course shows
// past it, as with ball.cpp's and backdrop.cpp's rectangles. The mask's texels past the 4:3 frame's sides are what the
// console would show there: black while the shape is inside the frame.
//
// The mask gives RT64 no scissor of its own. RT64 judges a frame by the scissors drawn in it: it keeps a frame's 2D at
// its own aspect only while they are 4:3-shaped, and widens the 3D only where its scissor spans theirs. A scissor out
// to the window's edges (raw coordinates past the 4:3 frame's sides) made each frame of a wipe wide-shaped, and RT64
// stretched its 2D across the window: Course Select, the main menu, the hole's name and the HUD, which jumped back to
// their own shape as the wipe ended. One as wide as the 4:3 frame (0 to 320, which the 3D's 8 to 312 doesn't span) held
// the course at 4:3 through the wipe, the sides black. Its sides given as the window's (G_EX_ORIGIN_LEFT and
// G_EX_ORIGIN_RIGHT) land at the HUD's edges instead, which HUD Ratio's Clamp 16:9 holds in from a wider window's, and
// the course showed in a strip down each side. With Aspect Ratio Original, or a window no wider than 4:3, nothing
// changes. The buffers are this file's in RDRAM, above the 4 MB the game uses.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_DL_NOPUSH = 1;

// The mask's rectangle, in quarter pixels.
constexpr uint32_t MASK_LEFT = 8 * 4, MASK_RIGHT = 312 * 4, MASK_TOP = 0, MASK_BOTTOM = 272 * 4;
constexpr int SCREEN_WIDTH = 320;

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

// The rectangle at addr (its three commands) replaced by a branch to a buffer that draws it stretched to the picture,
// scale times as wide as the 4:3 frame, with its texture step across scale times as large, and branches back after it.
void widen(uint32_t addr, float scale) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer };
    GfxCommand c[3];

    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t st = rd_w(addr + 12), d = rd_w(addr + 20);
    uint32_t tile = (w1 >> 24) & 7;
    int32_t s = int16_t(st >> 16), t = int16_t(st & 0xFFFF);
    int32_t dsdx = int16_t(d >> 16);
    uint32_t dtdy = d & 0xFFFF;
    // s at the screen's middle, then at the 4:3 frame's left edge with the larger step: dsdx is s5.10 texels a pixel, s
    // is s10.5, and the rectangle's coordinates are quarter pixels.
    int32_t wide_dsdx = int32_t(std::lround(dsdx * double(scale)));
    if (wide_dsdx > INT16_MAX) {
        wide_dsdx = INT16_MAX;
    }
    double s_middle = s + (SCREEN_WIDTH / 2.0 - MASK_LEFT / 4.0) * dsdx / 32.0;
    int32_t s_left = int32_t(std::lround(s_middle - (SCREEN_WIDTH / 2.0) * wide_dsdx / 32.0));
    if (s_left < INT16_MIN) {
        s_left = INT16_MIN;
    }
    if (s_left > INT16_MAX) {
        s_left = INT16_MAX;
    }

    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    gEXTextureRectangle(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, w1 & 0xFFF, SCREEN_WIDTH * 4, w0 & 0xFFF, tile,
                        s_left & 0xFFFF, t & 0xFFFF, wide_dsdx & 0xFFFF, dtdy);
    out.put(c, 3);
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
                widen(addr, scale);
            }
            break;
        }
        addr += 16;
        i += 2;
    }
}
