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
// mask's rectangle and steps its texture that many times faster across it (dsdx), its s moved back so that the texel
// at the screen's middle stays there. RT64 still stretches the rectangle across the window, now with the shape at its
// own aspect, and the mask's texels past the 4:3 frame are what the console would show there: black while the shape
// is inside the frame. So the black reaches the window's sides, however wide, where the course's 3D reaches them.
//
// The rectangle keeps the game's coordinates and scissor. A scissor of its own reaching past the frame's sides (an
// extended one, as this file once drew the mask under) changes how RT64 reads the whole frame: RT64 takes the widest
// scissor a frame draws under as the screen, and keeps at 4:3 only what covers that screen's width, so on each frame
// of the wipe everything else (the course, the hole's title over it, the HUD) was drawn stretched to the window, and
// snapped back as the wipe ended. With Aspect Ratio Original, or a window no wider than 4:3, nothing changes.

#include <cmath>
#include <cstdint>

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;

// The mask's rectangle, in quarter pixels.
constexpr uint32_t MASK_LEFT = 8 * 4, MASK_RIGHT = 312 * 4, MASK_TOP = 0, MASK_BOTTOM = 272 * 4;
constexpr int SCREEN_MIDDLE = 160;
// The commands read from a list, at most.
constexpr uint32_t MAX_COMMANDS = 0x4000;

uint8_t* g_rdram;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

// The game's mask, which it draws with dsdx and dtdy equal: once rewritten here they differ, so a list read twice
// isn't rewritten twice.
bool is_mask(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    if ((w0 >> 24) != G_TEXRECT || (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
        return false;
    }
    uint32_t d = rd_w(addr + 20);
    return ((w1 >> 12) & 0xFFF) == MASK_LEFT && (w1 & 0xFFF) == MASK_TOP && ((w0 >> 12) & 0xFFF) == MASK_RIGHT &&
           (w0 & 0xFFF) == MASK_BOTTOM && (d >> 16) == (d & 0xFFFF);
}

// The rectangle at addr (its three commands) drawn scale times as many texels a pixel across, in place. A pixel x of
// the rectangle lands, stretched, at 160 + (x - 160) * scale in the 4:3 frame's pixels, which should show the texel the
// game puts at that place: s + (160 + (x - 160) * scale - 8) * dsdx. So dsdx grows by scale, and s at the rectangle's
// left edge (x 8) moves back by 152 * (scale - 1) pixels' worth.
void unstretch(uint32_t addr, float scale) {
    uint32_t st = rd_w(addr + 12), d = rd_w(addr + 20);
    int32_t s = int16_t(st >> 16);
    uint32_t dsdx = d >> 16, dtdy = d & 0xFFFF;
    // dsdx is s5.10 texels a pixel, s is s10.5 texels.
    float wide_dsdx = std::round(float(dsdx) * scale);
    if (wide_dsdx > float(INT16_MAX)) {
        return;
    }
    float half = float(SCREEN_MIDDLE) - float(MASK_LEFT / 4);
    int32_t s_left = int32_t(std::lround(float(s) - half * (scale - 1.0f) * float(dsdx) / 32.0f));
    if (s_left < INT16_MIN) {
        s_left = INT16_MIN;
    }
    wr_w(addr + 12, (uint32_t(s_left & 0xFFFF) << 16) | (st & 0xFFFF));
    wr_w(addr + 20, (uint32_t(wide_dsdx) << 16) | dtdy);
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
                unstretch(addr, scale);
            }
            break;
        }
        addr += 16;
        i += 2;
    }
}
