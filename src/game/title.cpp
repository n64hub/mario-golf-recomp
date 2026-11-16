// The title screen's painting at the picture's full height in a wide window (RT64's Aspect Ratio Expand).
//
// The title (the painting with PRESS START over it) is a 2D screen: the game draws its painting as 38 strips of texture
// rectangles across x 8 to 312 and y 8 to 232, under a scissor of just that, with PRESS START a rectangle over it. RT64
// draws a 2D screen at its own 4:3 in the middle of a wider picture, so with Expand it stood in black on all four sides:
// the window's sides past 4:3, and the game's own 8-line border above and below. Here it is scaled about the screen's
// middle by 240/224, keeping its shape, until it fills the picture's height; its sides end at the 4:3 frame's, so in a
// wide window the black is left only beside it, a little narrower than before.
//
// nuGfxTaskStart sends each frame's display list, and us.toml hooks its start: mg_title_fill reads the list it is given,
// finds the painting's scissor (8, 8 to 312, 232), and rewrites every texture rectangle after it in place, until the
// scissor next changes or the list ends: scaled, its edges on whole pixels, clipped to the 4:3 frame with its texture
// start moved to match. They are point sampled (scale_rect and mg_title_fill say why). The scissor becomes the 4:3
// frame's, by a branch to a buffer that sets it with RT64's rectangles kept at their aspect (G_EX_ASPECT_ADJUST): a
// rectangle as wide as its scissor would otherwise be stretched across the window. The command after the last
// rectangle branches to one that sets both back. The buffers are this file's in RDRAM, above the 4 MB the game uses.
// The boot's logos are drawn under the same scissor, so the list is changed only where it holds the painting's strips.
// With Aspect Ratio Original, or a window no wider than 4:3, nothing changes.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETSCISSOR = 0xED, G_DL_NOPUSH = 1;
// G_SETOTHERMODE_H's texture filter (G_MDSFT_TEXTFILT), and its point and bilinear values.
constexpr uint32_t SET_TEXTURE_FILTER = 0xE3001201, G_TF_POINT = 0, G_TF_BILERP = 0x2000;

// The painting's scissor: 8, 8 to 312, 232, in quarter pixels.
constexpr uint32_t TITLE_SCISSOR_W0 = 0xED020020, TITLE_SCISSOR_W1 = 0x004E03A0;
// The 4:3 frame's scissor, 0, 0 to 320, 240.
constexpr uint32_t FRAME_SCISSOR_W0 = 0xED000000, FRAME_SCISSOR_W1 = 0x005003C0;
constexpr float SCREEN_WIDTH = 320.0f, SCREEN_HEIGHT = 240.0f;
// The painting, 8, 8 to 312, 232, its height scaled to the screen's; at least this many of its strips.
constexpr uint32_t PAINTING_LEFT = 8, PAINTING_TOP = 8, PAINTING_RIGHT = 312, PAINTING_BOTTOM = 232;
constexpr int MIN_STRIPS = 30;
constexpr float FILL = SCREEN_HEIGHT / float(PAINTING_BOTTOM - PAINTING_TOP);

// Above backdrop.cpp's buffers. Two a frame, and the renderer may read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80760000;
constexpr uint32_t BUFFER_SIZE = 0x40;
constexpr uint32_t BUFFER_COUNT = 16;
// The commands read from a list, and the rectangles after the painting's scissor, at most.
constexpr uint32_t MAX_COMMANDS = 0x4000;
constexpr int MAX_RECTS = 64;

uint8_t* g_rdram;
uint32_t g_next_buffer = 0;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

// The command at addr replaced by a branch to a buffer holding the rectangles' aspect, the texture filter if one is
// given, and then the command itself (or the scissor given), which branches back after it.
void replace(uint32_t addr, uint32_t aspect, uint32_t w0, uint32_t w1, bool bilerp = false) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    uint32_t pos = buffer;
    auto put = [&pos](uint32_t a, uint32_t b) {
        wr_w(pos, a);
        wr_w(pos + 4, b);
        pos += 8;
    };
    GfxCommand c[1];
    gEXEnable(c);
    put(c[0].values.word0, c[0].values.word1);
    gEXSetRectAspect(c, aspect);
    put(c[0].values.word0, c[0].values.word1);
    if (bilerp) {
        put(SET_TEXTURE_FILTER, G_TF_BILERP);
    }
    put(w0, w1);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (addr + 8) & 0x1FFFFFFF);
    wr_w(addr, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

float scale_x(float x) {
    return SCREEN_WIDTH / 2 + (x - SCREEN_WIDTH / 2) * FILL;
}

float scale_y(float y) {
    return SCREEN_HEIGHT / 2 + (y - SCREEN_HEIGHT / 2) * FILL;
}

// The rectangle at addr (its three commands) scaled, clipped to the 4:3 frame, in place.
void scale_rect(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    float ulx = ((w1 >> 12) & 0xFFF) / 4.0f, uly = (w1 & 0xFFF) / 4.0f;
    float lrx = ((w0 >> 12) & 0xFFF) / 4.0f, lry = (w0 & 0xFFF) / 4.0f;
    float s = int16_t(st >> 16) / 32.0f, t = int16_t(st & 0xFFFF) / 32.0f;
    float dsdx = int16_t(d >> 16) / 1024.0f, dtdy = int16_t(d & 0xFFFF) / 1024.0f;

    // On whole pixels: a rectangle's texture starts at the first whole pixel it covers, so a strip starting between two
    // would draw its rows up to a line off. Each edge is the first whole pixel at or past it, the same for both strips
    // that meet there, so they still meet, and every pixel a strip covers reads its own rows of the texture.
    float x0 = scale_x(ulx), y0 = scale_y(uly);
    auto edge = [](float v, float limit) { return int(std::ceil(std::fmin(std::fmax(v, 0.0f), limit) - 0.001f)); };
    int new_ulx = edge(x0, SCREEN_WIDTH), new_uly = edge(y0, SCREEN_HEIGHT);
    int new_lrx = edge(scale_x(lrx), SCREEN_WIDTH), new_lry = edge(scale_y(lry), SCREEN_HEIGHT);
    float new_dsdx = dsdx / FILL, new_dtdy = dtdy / FILL;
    float new_s = s + (new_ulx - x0) * new_dsdx;
    float new_t = t + (new_uly - y0) * new_dtdy;
    new_ulx *= 4;
    new_uly *= 4;
    new_lrx *= 4;
    new_lry *= 4;

    wr_w(addr, (w0 & 0xFF000000) | (uint32_t(new_lrx) << 12) | uint32_t(new_lry));
    wr_w(addr + 4, (w1 & 0xFF000000) | (uint32_t(new_ulx) << 12) | uint32_t(new_uly));
    wr_w(addr + 12, (uint32_t(int(std::lround(new_s * 32))) << 16) | (uint32_t(int(std::lround(new_t * 32))) & 0xFFFF));
    wr_w(addr + 20, (uint32_t(int(std::lround(new_dsdx * 1024))) << 16) |
                        (uint32_t(int(std::lround(new_dtdy * 1024))) & 0xFFFF));
}

} // namespace

extern "C" void mg_title_fill(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) != 0) {
        return;
    }
    // The painting's scissor, and the rectangles after it until the scissor changes or the list ends.
    uint32_t scissor = 0, filter = 0;
    uint32_t rects[MAX_RECTS];
    int rect_count = 0;
    for (uint32_t i = 0, addr = list; i < MAX_COMMANDS; i++, addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        if (op == G_ENDDL) {
            break;
        }
        if (w0 == SET_TEXTURE_FILTER && rect_count == 0) {
            filter = addr;
        }
        if (scissor == 0) {
            if (w0 == TITLE_SCISSOR_W0 && w1 == TITLE_SCISSOR_W1) {
                scissor = addr;
            }
            continue;
        }
        if (op == G_SETSCISSOR || op == G_DL) {
            break;
        }
        if (op == G_TEXRECT && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 && (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
            if (rect_count == MAX_RECTS) {
                return;
            }
            rects[rect_count++] = addr;
            addr += 16;
            i += 2;
        }
    }
    if (scissor == 0 || rect_count == 0) {
        return;
    }

    // The boot's logos are drawn under the same scissor, so it is the painting only if its strips span the scissor's
    // width, and everything else lies inside them.
    int strips = 0;
    for (int i = 0; i < rect_count; i++) {
        uint32_t w0 = rd_w(rects[i]), w1 = rd_w(rects[i] + 4);
        uint32_t ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF, lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
        if (uly < PAINTING_TOP * 4 || lry > PAINTING_BOTTOM * 4 || uly >= lry || ulx < PAINTING_LEFT * 4 ||
            lrx > PAINTING_RIGHT * 4 || ulx >= lrx || uint16_t(rd_w(rects[i] + 20) >> 16) == 0) {
            return;
        }
        if (ulx == PAINTING_LEFT * 4 && lrx == PAINTING_RIGHT * 4) {
            strips++;
        }
    }
    // Read only here: the boot's lists come before the window does.
    if (strips < MIN_STRIPS || mg_wide_scale() <= 1.0f) {
        return;
    }

    for (int i = 0; i < rect_count; i++) {
        scale_rect(rects[i]);
    }
    // Point sampled: the game filters them bilinearly, which at 1:1 reads each texel as it is, but scaled it blurs
    // each strip's rows into each other and stops at its edges (a strip's texture is only its own rows), so every strip
    // showed as a band. Point sampled they are the painting's own pixels, as the game drew them.
    bool bilerp = filter != 0 && rd_w(filter + 4) == G_TF_BILERP;
    if (bilerp) {
        wr_w(filter + 4, G_TF_POINT);
    }
    // Set back after the last rectangle, where the command is one that stands alone.
    uint32_t after = rects[rect_count - 1] + 24;
    uint32_t after_w0 = rd_w(after), after_w1 = rd_w(after + 4);
    uint32_t after_op = after_w0 >> 24;
    if (after_op != G_DL && after_op != G_ENDDL && after_op != G_TEXRECT && after_op != G_RDPHALF_1 &&
        after_op != G_RDPHALF_2) {
        replace(after, G_EX_ASPECT_AUTO, after_w0, after_w1, bilerp);
    }
    replace(scissor, G_EX_ASPECT_ADJUST, FRAME_SCISSOR_W0, FRAME_SCISSOR_W1);
}
