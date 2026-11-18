// The title and the menus at the picture's full height (RT64's Aspect Ratio Expand): a 2D screen fills the window's
// height, or its width where the window is no wider than the screen, its shape kept, centred, the rest black.
//
// The game draws every screen inside a border of its own: under a scissor of 8, 8 to 312, 232, the 304x224 a TV's edge
// left in sight. On the course RT64 widens the 3D past it to the window's sides, but the title (its painting, 38 strips
// of texture rectangles) and the menus (each a backdrop of texture rectangles, 11 strips across that scissor, a sign,
// text and pictures over it as more rectangles, and the play modes' golfer, a model) are 2D screens, which RT64 keeps
// at 4:3 in the middle of a wider picture: so they stood in a 912x672 box in a 1280x720 window, black above and below
// as well as beside them.
//
// Here each such screen is scaled about the screen's middle until its 224 lines fill the picture's 240, by 240/224, or
// in a window no wider than 4:3 until its 304 columns fill the 320, by 320/304, whichever is less. nuGfxTaskStart sends
// each frame's display list, and us.toml hooks its start: mg_screen_fill reads the list it is given, while the game is
// on the title (overlay 8 loaded) or in its menus (overlay 3), from the main menu to Course Select. In the list's own
// commands (the frame's, which the game writes anew each frame; the lists it calls are its data and are left alone):
// - each texture rectangle is scaled, its edges on whole pixels (scale_rect says why), and clipped to the 4:3 frame,
//   its texture start moved to match, and drawn through a buffer that keeps it at its shape
//   (gEXSetRectAspect(G_EX_ASPECT_ADJUST)): RT64 stretches a rectangle as wide as its scissor across the window, and a
//   scaled strip of the backdrop is as wide as the frame. They are point sampled: the game filters them bilinearly,
//   which at 1:1 reads each texel as it is, but scaled it blurs each strip's rows into each other and stops at its
//   edges (a strip's texture is only its own rows), so every strip of the title's painting showed as a band;
// - each scissor is scaled the same way and clipped to the frame, so the border's 8, 8 to 312, 232 becomes 0.25, 0 to
//   319.75, 240: a quarter pixel inside the frame's sides, or RT64 widens a model drawn under it (Course Select's sea)
//   to the window as it does the course's (scale_scissor). Nothing reaches past the frame's sides: RT64 takes the
//   widest scissor a frame draws under as the screen (iris.cpp says what one past the sides did), so in a wide window
//   the screen's sides lose what scaling it to the full height takes them past the frame's, 2.9 of its 325.7 columns
//   at each side;
// - each viewport is a copy scaled the same way, so a model drawn on a menu (the golfer) grows with it.
// A rectangle that reads the copy of the frame before that a transition draws (card.cpp's, of the picture RT64 drew,
// already scaled, or the game's own in its z-buffer) is left as it is, and so is one iris.cpp has already sent through
// a buffer of its own, so us.toml calls mg_iris_fix first. The buffers are this file's in RDRAM, above the 4 MB the game
// uses. With Aspect Ratio Original nothing changes.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();
bool mg_aspect_expand();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETSCISSOR = 0xED, G_MOVEMEM = 0xDC, G_SETTIMG = 0xFD, G_SETOTHERMODE_H = 0xE3;
constexpr uint32_t G_VTX = 0x01, G_TRI1 = 0x05, G_TRI2 = 0x06, G_QUAD = 0x07, G_DL_NOPUSH = 1;
// G_MOVEMEM of a viewport (G_MV_VIEWPORT, 16 bytes).
constexpr uint32_t MOVEMEM_VIEWPORT_W0 = 0xDC080008;
// G_SETOTHERMODE_H's texture filter (G_MDSFT_TEXTFILT) and its point value, and its cycle type (G_MDSFT_CYCLETYPE)
// and its copy value.
constexpr uint32_t SET_TEXTURE_FILTER = 0xE3001201, G_TF_POINT = 0;
constexpr uint32_t SET_CYCLE_TYPE = 0xE3000A01, G_CYC_COPY = 0x00200000;

// The overlay table (the game's loader sets the byte at the head of an overlay's entry while it is loaded), and the
// title's and the menus' overlays.
constexpr uint32_t OVERLAYS = 0x800B5F58, OVERLAY_SIZE = 0x28, MENU_OVERLAY = 3, TITLE_OVERLAY = 8;
// Where a transition's copy of the frame before lies in the game's RAM: its z-buffer (card.cpp's COPY_ADDR), 304x224
// at 16 bits. card.cpp's own copy is past the game's 8 MB.
constexpr uint32_t GAME_COPY = 0x002EA000, GAME_COPY_END = GAME_COPY + 304 * 224 * 2, GAME_RAM = 0x00800000;

constexpr float SCREEN_WIDTH = 320.0f, SCREEN_HEIGHT = 240.0f, CENTRE_X = 160.0f, CENTRE_Y = 120.0f;
// The 2D screen inside the game's border: 304x224.
constexpr float INNER_WIDTH = 304.0f, INNER_HEIGHT = 224.0f;

// Above backdrop.cpp's buffers and below ball.cpp's: 64 KB, 1024 of 64 bytes, used in turn. A menu draws up to ~200
// rectangles a frame, and the renderer may read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80760000;
constexpr uint32_t BUFFER_SIZE = 0x40;
constexpr uint32_t BUFFER_COUNT = 1024;
// The commands read from a list, at most.
constexpr uint32_t MAX_COMMANDS = 0x8000;

uint8_t* g_rdram;
uint32_t g_next_buffer = 0;
float g_fill = 1.0f;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

uint8_t rd_b(uint32_t addr) {
    return g_rdram[(addr & 0x7FFFFF) ^ 3];
}

bool loaded(uint32_t overlay) {
    return rd_b(OVERLAYS + overlay * OVERLAY_SIZE) != 0;
}

uint32_t next_buffer() {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    return buffer;
}

bool in_buffers(uint32_t addr) {
    addr &= 0x1FFFFFFF;
    return addr >= (BUFFER_BASE & 0x1FFFFFFF) && addr < ((BUFFER_BASE + BUFFER_SIZE * BUFFER_COUNT) & 0x1FFFFFFF);
}

float scale_x(float x) {
    return CENTRE_X + (x - CENTRE_X) * g_fill;
}

float scale_y(float y) {
    return CENTRE_Y + (y - CENTRE_Y) * g_fill;
}

// The first whole pixel at or past v, inside 0 to limit.
int edge(float v, float limit) {
    return int(std::ceil(std::fmin(std::fmax(v, 0.0f), limit) - 0.001f));
}

// The rectangle at addr (its three commands) scaled and clipped to the 4:3 frame, its commands written at out, false
// if nothing of it is left. A rectangle's texture starts at the first whole pixel it covers, so one starting between
// two would draw its rows up to a line off: each edge is the first whole pixel at or past it, the same for two
// rectangles that meet there, so they still meet, and every pixel one covers reads its own rows of the texture. In
// copy mode its lower right corner is the last pixel it covers, not the one past it.
bool scale_rect(uint32_t addr, uint32_t out, bool copy) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    float ulx = ((w1 >> 12) & 0xFFF) / 4.0f, uly = (w1 & 0xFFF) / 4.0f;
    float lrx = ((w0 >> 12) & 0xFFF) / 4.0f, lry = (w0 & 0xFFF) / 4.0f;
    if (copy) {
        lrx += 1.0f;
        lry += 1.0f;
    }
    float s = int16_t(st >> 16) / 32.0f, t = int16_t(st & 0xFFFF) / 32.0f;
    float dsdx = int16_t(d >> 16) / 1024.0f, dtdy = int16_t(d & 0xFFFF) / 1024.0f;

    float x0 = scale_x(ulx), y0 = scale_y(uly);
    int new_ulx = edge(x0, SCREEN_WIDTH), new_uly = edge(y0, SCREEN_HEIGHT);
    int new_lrx = edge(scale_x(lrx), SCREEN_WIDTH), new_lry = edge(scale_y(lry), SCREEN_HEIGHT);
    if (new_lrx <= new_ulx || new_lry <= new_uly) {
        return false;
    }
    float new_dsdx = dsdx / g_fill, new_dtdy = dtdy / g_fill;
    float new_s = s + (new_ulx - x0) * new_dsdx;
    float new_t = t + (new_uly - y0) * new_dtdy;
    if (copy) {
        new_lrx -= 1;
        new_lry -= 1;
    }

    wr_w(out, (w0 & 0xFF000000) | (uint32_t(new_lrx * 4) << 12) | uint32_t(new_lry * 4));
    wr_w(out + 4, (w1 & 0xFF000000) | (uint32_t(new_ulx * 4) << 12) | uint32_t(new_uly * 4));
    wr_w(out + 8, rd_w(addr + 8));
    wr_w(out + 12, (uint32_t(int(std::lround(new_s * 32))) << 16) | (uint32_t(int(std::lround(new_t * 32))) & 0xFFFF));
    wr_w(out + 16, rd_w(addr + 16));
    wr_w(out + 20, (uint32_t(int(std::lround(new_dsdx * 1024))) << 16) |
                       (uint32_t(int(std::lround(new_dtdy * 1024))) & 0xFFFF));
    return true;
}

// The rectangle at addr replaced by a branch to a buffer that draws it scaled at its shape and branches back after it.
// One scaled to nothing is skipped.
void replace_rect(uint32_t addr, bool copy) {
    uint32_t buffer = next_buffer();
    uint32_t pos = buffer;
    auto put = [&pos](uint32_t a, uint32_t b) {
        wr_w(pos, a);
        wr_w(pos + 4, b);
        pos += 8;
    };
    GfxCommand c[1];
    gEXEnable(c);
    put(c[0].values.word0, c[0].values.word1);
    gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
    put(c[0].values.word0, c[0].values.word1);
    if (scale_rect(addr, pos, copy)) {
        pos += 24;
    }
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    put(c[0].values.word0, c[0].values.word1);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (addr + 24) & 0x1FFFFFFF);
    wr_w(addr, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

// The scissor at addr scaled and clipped to the frame, in place. One the game set inside the frame's sides stays a
// quarter pixel inside them: RT64 widens a model to the window when its scissor spans the whole width the frame drew
// in, which the frame's clear makes the frame's, and a menu's models (Course Select's sea) are kept at the screen's
// shape as before.
void scale_scissor(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    float ulx = ((w0 >> 12) & 0xFFF) / 4.0f, uly = (w0 & 0xFFF) / 4.0f;
    float lrx = ((w1 >> 12) & 0xFFF) / 4.0f, lry = (w1 & 0xFFF) / 4.0f;
    uint32_t new_ulx = uint32_t(edge(scale_x(ulx), SCREEN_WIDTH)) * 4, new_uly = uint32_t(edge(scale_y(uly), SCREEN_HEIGHT)) * 4;
    uint32_t new_lrx = uint32_t(edge(scale_x(lrx), SCREEN_WIDTH)) * 4, new_lry = uint32_t(edge(scale_y(lry), SCREEN_HEIGHT)) * 4;
    if (ulx > 0.0f && new_ulx < 1) {
        new_ulx = 1;
    }
    if (lrx < SCREEN_WIDTH && new_lrx > SCREEN_WIDTH * 4 - 1) {
        new_lrx = uint32_t(SCREEN_WIDTH * 4 - 1);
    }
    wr_w(addr, (w0 & 0xFF000000) | (new_ulx << 12) | new_uly);
    wr_w(addr + 4, (w1 & 0xFF000000) | (new_lrx << 12) | new_lry);
}

// The viewport the G_MOVEMEM at addr loads, copied to a buffer, scaled about the screen's middle, and loaded from there.
// A viewport is four shorts of scale and four of translation, in quarter pixels.
void scale_viewport(uint32_t addr) {
    uint32_t vp = rd_w(addr + 4);
    if ((vp & 0x1FFFFFFF) >= GAME_RAM || in_buffers(vp)) {
        return;
    }
    vp |= 0x80000000;
    uint32_t sx_sy = rd_w(vp), sz = rd_w(vp + 4), tx_ty = rd_w(vp + 8), tz = rd_w(vp + 12);
    auto sh = [](uint32_t w, int hi) { return float(int16_t(hi ? (w >> 16) : (w & 0xFFFF))); };
    auto pack = [](float a, float b) {
        return (uint32_t(int(std::lround(a))) << 16) | (uint32_t(int(std::lround(b))) & 0xFFFF);
    };
    float sx = sh(sx_sy, 1) * g_fill, sy = sh(sx_sy, 0) * g_fill;
    float tx = CENTRE_X * 4 + (sh(tx_ty, 1) - CENTRE_X * 4) * g_fill;
    float ty = CENTRE_Y * 4 + (sh(tx_ty, 0) - CENTRE_Y * 4) * g_fill;
    uint32_t buffer = next_buffer();
    wr_w(buffer, pack(sx, sy));
    wr_w(buffer + 4, sz);
    wr_w(buffer + 8, pack(tx, ty));
    wr_w(buffer + 12, tz);
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

bool is_rect(uint32_t addr) {
    return (rd_w(addr) >> 24) == G_TEXRECT && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
           (rd_w(addr + 16) >> 24) == G_RDPHALF_2;
}

// Whether the next thing drawn after addr is a texture rectangle, not a model.
bool rect_next(uint32_t addr, uint32_t end) {
    for (addr += 8; addr < end; addr += 8) {
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_TEXRECT) {
            return true;
        }
        if (op == G_VTX || op == G_TRI1 || op == G_TRI2 || op == G_QUAD || op == G_DL || op == G_ENDDL) {
            return false;
        }
    }
    return false;
}

} // namespace

extern "C" void mg_screen_fill(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) != 0 || !mg_aspect_expand() || !(loaded(MENU_OVERLAY) || loaded(TITLE_OVERLAY))) {
        return;
    }
    // In a window no wider than 4:3 the frame fills its width; in a wider one, its height, and as much of the width.
    float width = SCREEN_WIDTH * mg_wide_scale();
    g_fill = std::fmin(SCREEN_HEIGHT / INNER_HEIGHT, width / INNER_WIDTH);
    if (g_fill <= 1.0f) {
        return;
    }

    uint32_t end = list;
    for (uint32_t i = 0; i < MAX_COMMANDS && (rd_w(end) >> 24) != G_ENDDL; i++) {
        end += 8;
    }
    if ((rd_w(end) >> 24) != G_ENDDL) {
        return;
    }

    uint32_t texture = 0;
    bool copy = false;
    for (uint32_t addr = list; addr < end; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        if (op == G_SETTIMG) {
            texture = w1 & 0x1FFFFFFF;
        }
        else if (w0 == SET_CYCLE_TYPE) {
            copy = (w1 == G_CYC_COPY);
        }
        else if (w0 == SET_TEXTURE_FILTER) {
            if (w1 != G_TF_POINT && rect_next(addr, end)) {
                wr_w(addr + 4, G_TF_POINT);
            }
        }
        else if (op == G_SETSCISSOR) {
            scale_scissor(addr);
        }
        else if (w0 == MOVEMEM_VIEWPORT_W0) {
            scale_viewport(addr);
        }
        else if (op == G_TEXRECT && is_rect(addr)) {
            bool copied_frame = texture >= GAME_RAM || (texture >= GAME_COPY && texture < GAME_COPY_END);
            if (!copied_frame) {
                replace_rect(addr, copy);
            }
            addr += 16;
        }
    }
}
