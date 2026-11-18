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
// mask's rectangle and sends it through a buffer that draws it stretched across the window (G_EX_ASPECT_STRETCH, as
// RT64 draws it on its own where the game's scissor is the frame's, but on a 4:3 menu too), its texture stepped that
// many times faster across it (dsdx) and its s moved back so that the texel at the screen's middle stays there. So the
// shape keeps its own aspect, and the mask's texels past the 4:3 frame are what the console would show there: black
// while the shape is inside the frame, out to the window's sides however wide, where the course's 3D reaches them.
// The rectangle runs from the screen's left edge to its right one (0 to 320), past the game's scissor (8 to 312), which
// cuts it where it cuts the course: RT64 places the left edge of a rectangle it stretches a pixel or two right of the
// stretched scissor's, and one from 8 to 312 left a column of the course showing at each side (hud.cpp's
// spanning_rect_from_left, the same for Lakitu Mode's letterbox bars).
//
// The rectangle keeps the game's coordinates and scissor. A scissor of its own reaching past the frame's sides (an
// extended one, as this file once drew the mask under) changes how RT64 reads the whole frame: RT64 takes the widest
// scissor a frame draws under as the screen, and keeps at 4:3 only what covers that screen's width, so on each frame
// of the wipe everything else (the course, the hole's title over it, the HUD) was drawn stretched to the window, and
// snapped back as the wipe ended. The buffers are this file's in RDRAM, above the 4 MB the game uses. With Aspect Ratio
// Original, or a window no wider than 4:3, nothing changes.

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
constexpr int SCREEN_MIDDLE = SCREEN_WIDTH / 2;
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

// The rectangle at addr (its three commands) replaced by a branch to a buffer that draws it from the screen's left
// edge to its right one, stretched across the window with scale times as many texels a pixel across, and branches back
// after it. A pixel x of the rectangle lands, stretched, at 160 + (x - 160) * scale in the 4:3 frame's pixels, which
// should show the texel the game puts at that place: s + (160 + (x - 160) * scale - 8) * dsdx. So dsdx grows by scale,
// and s at the rectangle's new left edge (x 0) is the game's moved back by 160 * (scale - 1) + 8 pixels' worth.
void stretch(uint32_t addr, float scale) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t st = rd_w(addr + 12), d = rd_w(addr + 20);
    int32_t s = int16_t(st >> 16);
    uint32_t dsdx = d >> 16, dtdy = d & 0xFFFF;
    // dsdx is s5.10 texels a pixel, s is s10.5 texels.
    float wide_dsdx = std::round(float(dsdx) * scale);
    if (wide_dsdx > float(INT16_MAX)) {
        return;
    }
    float back = float(SCREEN_MIDDLE) * (scale - 1.0f) + float(MASK_LEFT / 4);
    int32_t s_left = int32_t(std::lround(float(s) - back * float(dsdx) / 32.0f));
    if (s_left < INT16_MIN) {
        s_left = INT16_MIN;
    }

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer };
    GfxCommand c[1];
    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    out.put((w0 & ~(0xFFFu << 12)) | (uint32_t(SCREEN_WIDTH * 4) << 12), w1 & ~(0xFFFu << 12));
    out.put(rd_w(addr + 8), (uint32_t(s_left & 0xFFFF) << 16) | (st & 0xFFFF));
    out.put(rd_w(addr + 16), (uint32_t(wide_dsdx) << 16) | dtdy);
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
                stretch(addr, scale);
            }
            break;
        }
        addr += 16;
        i += 2;
    }
}
