// The iris wipe at its own shape in a wide window (RT64's Aspect Ratio Expand), over the border's sides in any window,
// and with Enhancements On its black surround solid.
//
// Between screens (Course Select into a hole, a hole's end) the game closes and opens an iris in the shape of a
// character's head over the picture: one texture rectangle from x 8 to 312 and y 0 to 272, a small mask texture drawn
// many times its size (dsdx and dtdy equal, from about 1/12 to 2 texels a pixel), black around the shape and see-through
// inside it, its s and t moved and scaled each frame to grow or shrink the shape about the screen's middle. RT64
// stretches a rectangle as wide as its scissor across the whole window, as it does a screen's fill, so in a wide window
// the shape was drawn wider than it is, by the window's width over a 4:3 one's.
//
// nuGfxTaskStart sends each frame's display list, and the hook there calls mg_iris_fix with it, before screen.cpp reads it: it finds the
// mask's rectangle and sends it through a buffer that draws it stretched across the window (G_EX_ASPECT_STRETCH, as
// RT64 draws it on its own where the game's scissor is the frame's, but on a 4:3 menu too), its texture stepped that
// many times faster across it (dsdx) and its s moved back so that the texel at the screen's middle stays there. So the
// shape keeps its own aspect, and the mask's texels past the 4:3 frame are what the console would show there: black
// while the shape is inside the frame, out to the window's sides however wide, where the course's 3D reaches them.
// The rectangle runs from the screen's left edge to its right one (0 to 320), past the game's scissor (8 to 312), which
// cuts it where it cuts the course: RT64 places the left edge of a rectangle it stretches a pixel or two right of the
// stretched scissor's, and one from 8 to 312 left a column of the course showing at each side (hud.cpp's
// spanning_rect_from_left, the same for Lakitu Mode's letterbox bars). A window no wider than 4:3 gets the same
// rectangle, unstretched (scale 1): on a hole border.cpp makes the game's scissor the screen's, and one from 8 to 312
// left the course showing in the border's 8 pixels at each side through the wipe.
//
// The mask is a 64x64 I4 texture, clamped in s and t, so past its edges each row and column repeats its last texel.
// Where the shape's rim reaches an edge, that texel isn't the surround's black: Koopa Park's shell, at its widest
// rows (36 to 39), ends in 0xE at each side, one step short of black, so a band of the picture behind showed through
// the black at a fifteenth of its brightness, from the shell to the screen's sides, a console's 4:3 frame included.
// With the General tab's Enhancements On (src/game/config.cpp) the buffer draws over the rectangle up to four more
// round the texture's pixels (the tile's size, from the list's last G_SETTILESIZE for its tile), each of their pixels
// sampling the texture's first texel, a corner, which every mask has black. The rectangle stays whole under them: drawn
// as the pixels inside the texture and four round them, RT64's frames between the game's at the display's rate now and
// then left a gap between the pieces, the course showing in a ring round the shape. Original draws the band as a
// console does.
//
// The rectangle keeps the game's coordinates and scissor. A scissor of its own reaching past the frame's sides (an
// extended one, as this file once drew the mask under) changes how RT64 reads the whole frame: RT64 takes the widest
// scissor a frame draws under as the screen, and keeps at 4:3 only what covers that screen's width, so on each frame
// of the wipe everything else (the course, the hole's title over it, the HUD) was drawn stretched to the window, and
// snapped back as the wipe ended. The buffers are this file's in RDRAM, above the 4 MB the game uses.

#include <algorithm>
#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"
#include "mg.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETTILESIZE = 0xF2;
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

// A texture's extent in s or t (texels): its first texel and the one past its last.
struct Extent {
    float first, end;
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

// A texture rectangle on the game's tile from x0 to x1 and y0 to y1 (pixels), sampling s and t (texels) at its top
// left and stepping dsdx and dtdy texels a pixel. Nothing if it's empty.
void put_rect(Writer& out, uint32_t tile, float x0, float x1, float y0, float y1, float s, float t, float dsdx,
              float dtdy) {
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    auto pixels = [](float v) { return uint32_t(std::lround(v * 4.0f)) & 0xFFF; };
    auto fixed = [](float v, float one) {
        return uint32_t(std::clamp<long>(std::lround(v * one), INT16_MIN, INT16_MAX)) & 0xFFFF;
    };
    out.put((G_TEXRECT << 24) | (pixels(x1) << 12) | pixels(y1), (tile << 24) | (pixels(x0) << 12) | pixels(y0));
    out.put(G_RDPHALF_1 << 24, (fixed(s, 32.0f) << 16) | fixed(t, 32.0f));
    out.put(G_RDPHALF_2 << 24, (fixed(dsdx, 1024.0f) << 16) | fixed(dtdy, 1024.0f));
}

// The pixel at which a rectangle from p0 to p1, sampling c0 at p0 and stepping d a pixel, samples c, kept inside it.
float pixel_at(float c, float p0, float p1, float c0, float d) {
    return std::clamp(p0 + (c - c0) / d, p0, p1);
}

// The rectangle at addr (its three commands) replaced by a branch to a buffer that draws it and branches back after it.
// It is drawn from the screen's left edge to its right one, stretched across the window with scale (1 in a window no
// wider than 4:3) times as many texels a pixel across. A pixel x of the rectangle lands, stretched, at 160 + (x - 160) *
// scale in the 4:3 frame's pixels, which should show the texel the game puts at that place: s + (160 + (x - 160) *
// scale - 8) * dsdx. So dsdx grows by scale, and s at the rectangle's new left edge (x 0) is the game's moved back by
// 160 * (scale - 1) + 8 pixels' worth. With Enhancements On and the tile's size known, the surround goes over it (the
// header's paragraph on the mask's texture).
void draw(uint32_t addr, float scale, const Extent* s_extent, const Extent* t_extent) {
    uint32_t tile = (rd_w(addr + 4) >> 24) & 7;
    uint32_t st = rd_w(addr + 12), d = rd_w(addr + 20);
    // s and t are s10.5 texels, dsdx and dtdy s5.10 texels a pixel.
    float s = float(int16_t(st >> 16)) / 32.0f, t = float(int16_t(st & 0xFFFF)) / 32.0f;
    float dsdx = float(int16_t(d >> 16)) / 1024.0f, dtdy = float(int16_t(d & 0xFFFF)) / 1024.0f;
    float x0 = 0.0f, x1 = float(SCREEN_WIDTH);
    float y0 = float(MASK_TOP) / 4.0f, y1 = float(MASK_BOTTOM) / 4.0f;
    scale = std::max(scale, 1.0f);
    float wide_dsdx = std::round(dsdx * 1024.0f * scale);
    if (wide_dsdx > float(INT16_MAX)) {
        return;
    }
    float back = float(SCREEN_MIDDLE) * (scale - 1.0f) + float(MASK_LEFT) / 4.0f;
    s = std::max(std::round((s - back * dsdx) * 32.0f), float(INT16_MIN)) / 32.0f;
    dsdx = wide_dsdx / 1024.0f;
    bool surround = s_extent != nullptr && dsdx > 0.0f && dtdy > 0.0f && mg::enhancements();

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer };
    GfxCommand c[1];
    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    put_rect(out, tile, x0, x1, y0, y1, s, t, dsdx, dtdy);
    if (surround) {
        // Over it, the surround on the texture's corner texel, from whole pixels inside the texture's edges out.
        float left = std::ceil(pixel_at(s_extent->first, x0, x1, s, dsdx));
        float right = std::max(left, std::floor(pixel_at(s_extent->end, x0, x1, s, dsdx)));
        float top = std::ceil(pixel_at(t_extent->first, y0, y1, t, dtdy));
        float bottom = std::max(top, std::floor(pixel_at(t_extent->end, y0, y1, t, dtdy)));
        float s_corner = s_extent->first, t_corner = t_extent->first;
        put_rect(out, tile, x0, left, y0, y1, s_corner, t_corner, 0.0f, 0.0f);
        put_rect(out, tile, right, x1, y0, y1, s_corner, t_corner, 0.0f, 0.0f);
        put_rect(out, tile, left, right, y0, top, s_corner, t_corner, 0.0f, 0.0f);
        put_rect(out, tile, left, right, bottom, y1, s_corner, t_corner, 0.0f, 0.0f);
    }
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
    // Each tile's size as the list last set it.
    Extent s_extent[8], t_extent[8];
    bool sized[8] = {};
    for (uint32_t i = 0, addr = list; i < MAX_COMMANDS; i++, addr += 8) {
        uint32_t w0 = rd_w(addr);
        uint32_t op = w0 >> 24;
        if (op == G_ENDDL) {
            break;
        }
        if (op == G_SETTILESIZE) {
            // Its corners in quarter texels, the last one's texel included.
            uint32_t w1 = rd_w(addr + 4), tile = (w1 >> 24) & 7;
            s_extent[tile] = { float((w0 >> 12) & 0xFFF) / 4.0f, float((w1 >> 12) & 0xFFF) / 4.0f + 1.0f };
            t_extent[tile] = { float(w0 & 0xFFF) / 4.0f, float(w1 & 0xFFF) / 4.0f + 1.0f };
            sized[tile] = true;
            continue;
        }
        if (op != G_TEXRECT) {
            continue;
        }
        if (is_mask(addr)) {
            uint32_t tile = (rd_w(addr + 4) >> 24) & 7;
            // Read only here, once a list holds the mask.
            draw(addr, mg_wide_scale(), sized[tile] ? &s_extent[tile] : nullptr,
                 sized[tile] ? &t_extent[tile] : nullptr);
            break;
        }
        addr += 16;
        i += 2;
    }
}
