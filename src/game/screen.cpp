// The title and the menus at the picture's full height (RT64's Aspect Ratio Expand): a 2D screen fills the 4:3 frame's
// height in any window, its shape kept, centred, the rest black.
//
// The game draws every screen inside a border of its own: under a scissor of 8, 8 to 312, 232, the 304x224 a TV's edge
// left in sight. On the course RT64 widens the 3D past it to the window's sides, but the title (its painting, 38 strips
// of texture rectangles) and the menus (each a backdrop of texture rectangles, 11 strips across that scissor, a sign,
// text and pictures over it as more rectangles, and the play modes' golfer, a model) are 2D screens, which RT64 keeps
// at 4:3 in the middle of a wider picture: so they stood in a 912x672 box in a 1280x720 window, black above and below
// as well as beside them.
//
// Here each such screen is scaled about the screen's middle until its 224 lines fill the picture's 240, by 240/224, in
// every window, 4:3 and narrower too: filling its 304 columns to the 320 there instead (by 320/304) left 235.8 of the
// 240 lines, a black band of about 1% above and below that a TV's edge hid. nuGfxTaskStart sends
// each frame's display list, and us.toml hooks its start: mg_screen_fill reads the list it is given, while the game is
// on the title (overlay 8 loaded) or in its menus (overlay 3), from the main menu to Course Select. In the list's own
// commands (the frame's, which the game writes anew each frame; the lists it calls are its data and are left alone):
// - each texture rectangle is scaled, its edges on whole pixels (scale_rect says why), and clipped to the 4:3 frame,
//   its texture start moved to match, and drawn through a buffer that keeps it at its shape
//   (gEXSetRectAspect(G_EX_ASPECT_ADJUST)): RT64 stretches a rectangle as wide as its scissor across the window, and a
//   scaled strip of the backdrop is as wide as the frame. They are point sampled: the game filters them bilinearly,
//   which at 1:1 reads each texel as it is, but scaled it blurs each strip's rows into each other and stops at its
//   edges (a strip's texture is only its own rows), so every strip of the title's painting showed as a band;
// - each scissor is scaled the same way and clipped to the frame, so the border's 8, 8 to 312, 232 becomes 0, 0 to
//   320, 240, or in a window wider than 4:3 0.25, 0 to 319.75, 240: a quarter pixel inside the frame's sides, or RT64 widens a model drawn under it (Course Select's sea)
//   to the window as it does the course's (scale_scissor). Nothing reaches past the frame's sides: RT64 takes the
//   widest scissor a frame draws under as the screen (iris.cpp says what one past the sides did), so in every window
//   the screen's sides lose what scaling it to the full height takes them past the frame's, 2.9 of its 325.7 columns
//   at each side, where the game's own border hid 8;
// - each viewport is a copy scaled the same way, so a model drawn on a menu (the golfer) grows with it.
// Only what is drawn to the screen is scaled: while the color image is another (G_SETCIMG of a width other than the
// screen's), the commands are left as the game wrote them. Metal Mario's shine on Play Modes is a 32x32 map the game
// draws each frame from rectangles, strips of the screen and a mix of textures, into a color image of its own, and
// scaled about the screen's middle they covered only its upper left 23x26, so whatever of him sampled the rest of the
// map drew black: nearly half his face, cap and body.
// A rectangle that reads the copy of the frame before that a transition draws (card.cpp's, of the picture RT64 drew,
// already scaled, or the game's own in its z-buffer) is left as it is, and so is one iris.cpp has already sent through
// a buffer of its own, so us.toml calls mg_iris_fix first. But for the wipe between the title and the main menu (both
// ways): the game draws the new screen whole and over it the old one, 64 tiles of 38x28 of its copy in the z-buffer
// (304x224, the frame inside its border, read by the CPU out of the framebuffer RT64 wrote back), in a spiral from the
// top left corner inwards, and each frame leaves out 4 more from the spiral's inner end, the last few fading, so the new
// screen opens from the middle (in 16 frames). Left at 4:3 while the new screen was scaled, the tiles left the new
// screen showing in a band round them from the wipe's first frame, where the old one filled the picture a frame before,
// and the old menu's sign and board stayed over the title to the wipe's end. So the wipe's first frame copies the frame
// before (card.cpp's mg_copy_frame_list: the whole picture, as RT64 drew it), and each tile is drawn at the place it is
// scaled to, reading that image at the same place (wipe_tile), so the old screen is the picture it was until its tiles
// go. RT64 draws the image at the window's width, so a tile reads its columns 1 / wide of a column apart, about the
// middle. The wipe's second frame is drawn while the game swaps the two overlays, neither of them loaded, so a wipe
// under way is scaled on through it and keeps its image to its last tile; let go of there, its tiles drew at 4:3 that
// frame, inside a black band, and from the next each copied that shrunk picture anew. The buffers are this file's in
// RDRAM, above the 4 MB the game uses. With Aspect Ratio Original nothing changes. The credits' last picture, a 2D
// screen drawn in a frame with 3D, goes through the same scaling from credits.cpp (mg_screen_fill_range).

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();
bool mg_aspect_expand();
extern "C" uint32_t mg_copy_frame_list(uint8_t* rdram, uint32_t from, uint32_t to, uint32_t* image);

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETSCISSOR = 0xED, G_MOVEMEM = 0xDC, G_SETTIMG = 0xFD, G_SETCIMG = 0xFF, G_SETOTHERMODE_H = 0xE3;
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
// The 2D screen inside the game's border: 304x224, of which its height is what the screen is scaled to fill.
constexpr float INNER_HEIGHT = 224.0f;

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
// Whether the window is wider than the 4:3 frame, where RT64 can widen a model past it.
bool g_wide = false;

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

// One side of a scaled rectangle, across (x) or down (y): from the game's edges a0 to a1 (screen pixels) and its
// texture's start there and step a pixel (texels), the first and past-the-last whole pixel it is drawn over, inside the
// frame, and the start and step (in the command's fixed point: 1/32 and 1/1024 of a texel) that read the same texels
// across them. Its texels are those the scaled edges cover, cut where the frame clips it, fitted to the whole pixels
// exactly, so the last pixel ends at the last texel and reads no further. Kept at the game's step, with its edges
// rounded out to whole pixels, a rectangle read up to most of a texel past its own, which the game's picture never
// shows, but RT64 draws the rectangle at the window's resolution and so that part of its last pixel too: a line of the
// next row or column of the texture, which in a font is the glyph beside it, round Character Select's text.
struct Side {
    int lo, hi;
    int start, step;
};

bool scale_side(bool across, float a0, float a1, float tex, float per_pixel, Side& out) {
    float limit = across ? SCREEN_WIDTH : SCREEN_HEIGHT;
    float b0 = across ? scale_x(a0) : scale_y(a0);
    float b1 = across ? scale_x(a1) : scale_y(a1);
    out.lo = edge(b0, limit);
    out.hi = edge(b1, limit);
    if (out.hi <= out.lo) {
        return false;
    }
    // The texel at a point of the scaled side, and the texels of the part inside the frame.
    float step = per_pixel / g_fill;
    float t0 = tex + (std::fmax(b0, 0.0f) - b0) * step;
    float t1 = tex + (std::fmin(b1, limit) - b0) * step;
    // Rounded towards the inside, so neither end reaches a texel past the range.
    out.start = int(step >= 0.0f ? std::ceil(t0 * 32.0f - 0.001f) : std::floor(t0 * 32.0f + 0.001f));
    out.step = int(std::trunc((t1 - out.start / 32.0f) / float(out.hi - out.lo) * 1024.0f));
    return true;
}

// The rectangle at addr (its three commands) scaled and clipped to the 4:3 frame, its commands written at out, false
// if nothing of it is left. Each edge is the first whole pixel at or past it, the same for two rectangles that meet
// there, so they still meet, and its texture is fitted to the pixels it covers (scale_side). In copy mode its lower
// right corner is the last pixel it covers, not the one past it, and its s step is four times its texels a pixel.
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
    float s_per_pixel = copy ? dsdx / 4.0f : dsdx;

    Side x, y;
    if (!scale_side(true, ulx, lrx, s, s_per_pixel, x) || !scale_side(false, uly, lry, t, dtdy, y)) {
        return false;
    }
    int new_dsdx = copy ? x.step * 4 : x.step;
    if (copy) {
        x.hi -= 1;
        y.hi -= 1;
    }

    wr_w(out, (w0 & 0xFF000000) | (uint32_t(x.hi * 4) << 12) | uint32_t(y.hi * 4));
    wr_w(out + 4, (w1 & 0xFF000000) | (uint32_t(x.lo * 4) << 12) | uint32_t(y.lo * 4));
    wr_w(out + 8, rd_w(addr + 8));
    wr_w(out + 12, (uint32_t(x.start) << 16) | (uint32_t(y.start) & 0xFFFF));
    wr_w(out + 16, rd_w(addr + 16));
    wr_w(out + 20, (uint32_t(new_dsdx) << 16) | (uint32_t(y.step) & 0xFFFF));
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
// shape as before. Only in a window wider than 4:3: RT64 draws the scissor from whole columns, so the quarter pixel
// left the frame's first column black, which there is beside the black past the frame, but in a 4:3 window was a
// column of black at the window's left edge, where nothing is widened.
void scale_scissor(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    float ulx = ((w0 >> 12) & 0xFFF) / 4.0f, uly = (w0 & 0xFFF) / 4.0f;
    float lrx = ((w1 >> 12) & 0xFFF) / 4.0f, lry = (w1 & 0xFFF) / 4.0f;
    uint32_t new_ulx = uint32_t(edge(scale_x(ulx), SCREEN_WIDTH)) * 4, new_uly = uint32_t(edge(scale_y(uly), SCREEN_HEIGHT)) * 4;
    uint32_t new_lrx = uint32_t(edge(scale_x(lrx), SCREEN_WIDTH)) * 4, new_lry = uint32_t(edge(scale_y(lry), SCREEN_HEIGHT)) * 4;
    if (g_wide && ulx > 0.0f && new_ulx < 1) {
        new_ulx = 1;
    }
    if (g_wide && lrx < SCREEN_WIDTH && new_lrx > SCREEN_WIDTH * 4 - 1) {
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

// Whether the next thing drawn after addr is a texture rectangle on the screen, not a model or something drawn into
// another color image.
bool rect_next(uint32_t addr, uint32_t end) {
    for (addr += 8; addr < end; addr += 8) {
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_TEXRECT) {
            return true;
        }
        if (op == G_VTX || op == G_TRI1 || op == G_TRI2 || op == G_QUAD || op == G_DL || op == G_ENDDL ||
            op == G_SETCIMG) {
            return false;
        }
    }
    return false;
}

// The tile wipe between the title and the main menu. The wipe's first frame copies the frame before (the whole picture,
// as RT64 drew it, scaled) into an image of card.cpp's; each tile then reads that image at its scaled place.
uint32_t g_cimg = 0;          // the color image in effect while the list is read
uint32_t g_last_cimg = 0;     // the screen's color image at the end of the frame before's list
bool g_tiles = false;         // whether this frame's list has the wipe's tiles
uint32_t g_wipe_image = 0;    // the image a wipe under way reads, 0 if none

// The commands of the tile whose G_SETTIMG of the game's copy is at at, rewritten to read the wipe's image at the place
// the tile is scaled to, and the address past its rectangle; 0 if it isn't a tile of that shape (left as it is). A tile
// is G_SETTIMG (RGBA16, 304 wide) at the copy, G_SETTILE 7, G_LOADTILE of its texels, G_SETTILE 0 and G_SETTILESIZE of
// the same, and a G_TEXRECT (its G_RDPHALF_1 and _2) of them at 1:1, the copy's texel (s, t) at the screen's (s + 8,
// t + 8); load and pipe syncs and a G_SETPRIMCOLOR (the fading tiles' alpha) between.
uint32_t wipe_tile(uint32_t at, uint32_t end) {
    uint32_t settile7 = 0, loadtile = 0, settile0 = 0, tilesize = 0, rect = 0;
    for (uint32_t a = at + 8; a < end && a < at + 8 * 16; a += 8) {
        uint32_t w0 = rd_w(a), w1 = rd_w(a + 4), op = w0 >> 24, tile = (w1 >> 24) & 7;
        if (op == 0xF5 && tile == 7 && settile7 == 0) {
            settile7 = a;
        }
        else if (op == 0xF5 && tile == 0 && loadtile != 0 && settile0 == 0) {
            settile0 = a;
        }
        else if (op == 0xF4 && tile == 7 && settile7 != 0 && loadtile == 0) {
            loadtile = a;
        }
        else if (op == 0xF2 && tile == 0 && settile0 != 0 && tilesize == 0) {
            tilesize = a;
        }
        else if (op == G_TEXRECT && tilesize != 0 && is_rect(a)) {
            rect = a;
            break;
        }
        else if (op != 0xE6 && op != 0xE7 && op != 0xFA) {
            return 0;
        }
    }
    if (rect == 0) {
        return 0;
    }
    uint32_t w0 = rd_w(rect), w1 = rd_w(rect + 4), st = rd_w(rect + 12), d = rd_w(rect + 20);
    uint32_t ulx = ((w1 >> 12) & 0xFFF) >> 2, uly = (w1 & 0xFFF) >> 2;
    uint32_t lrx = ((w0 >> 12) & 0xFFF) >> 2, lry = (w0 & 0xFFF) >> 2;
    if (d != 0x04000400 || ulx < 8 || uly < 8 || st != ((((ulx - 8) << 5) << 16) | ((uly - 8) << 5))) {
        return 0;
    }
    int x0 = edge(scale_x(float(ulx)), SCREEN_WIDTH), y0 = edge(scale_y(float(uly)), SCREEN_HEIGHT);
    int x1 = edge(scale_x(float(lrx)), SCREEN_WIDTH), y1 = edge(scale_y(float(lry)), SCREEN_HEIGHT);
    if (x1 <= x0 || y1 <= y0) {
        // Nothing of it is left: the rectangle is skipped.
        wr_w(rect, (G_DL << 24) | (G_DL_NOPUSH << 16));
        wr_w(rect + 4, (rect + 24) & 0x1FFFFFFF);
        return rect + 24;
    }
    GfxCommand c[1];
    uint32_t pos = 0;
    auto put = [&pos](uint32_t a, uint32_t b) {
        wr_w(pos, a);
        wr_w(pos + 4, b);
        pos += 8;
    };
    // The G_SETTIMG: the image, past the game's RAM (gEXSetRDRAMExtended), made first if this is the wipe's first tile.
    uint32_t buffer = next_buffer();
    pos = buffer;
    gEXEnable(c);
    put(c[0].values.word0, c[0].values.word1);
    gEXSetRDRAMExtended(c, 1);
    put(c[0].values.word0, c[0].values.word1);
    if (g_wipe_image == 0) {
        put(G_DL << 24, mg_copy_frame_list(g_rdram, g_last_cimg, g_cimg, &g_wipe_image));
    }
    put(0xFD10013F, g_wipe_image);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (at + 8) & 0x1FFFFFFF);
    wr_w(at, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(at + 4, buffer & 0x1FFFFFFF);
    // The load: the image's texels under the scaled tile, a row of them in TMEM's 8-byte words. RT64 draws the image at
    // the window's width, its 320 columns across all of it, so the 4:3 frame's column x is its 160 + (x - 160) / wide.
    float wide = mg_wide_scale();
    float s0 = CENTRE_X + (float(x0) - CENTRE_X) / wide;
    int sx0 = int(std::floor(s0)), sx1 = int(std::ceil(CENTRE_X + (float(x1) - CENTRE_X) / wide));
    if (sx1 <= sx0) {
        sx1 = sx0 + 1;
    }
    uint32_t line = (uint32_t(sx1 - sx0) * 2 + 7) / 8;
    wr_w(settile7, (rd_w(settile7) & ~(0x1FFu << 9)) | (line << 9));
    wr_w(settile0, (rd_w(settile0) & ~(0x1FFu << 9)) | (line << 9));
    uint32_t ul = (uint32_t(sx0) << 14) | (uint32_t(y0) << 2), lr = (uint32_t(sx1 - 1) << 14) | (uint32_t(y1 - 1) << 2);
    wr_w(loadtile, 0xF4000000 | ul);
    wr_w(loadtile + 4, 0x07000000 | lr);
    wr_w(tilesize, 0xF2000000 | ul);
    wr_w(tilesize + 4, lr);
    // The rectangle, at its shape, a texel of the image a pixel high and 1 / wide of one wide, and the game's RAM back
    // after it.
    buffer = next_buffer();
    pos = buffer;
    gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
    put(c[0].values.word0, c[0].values.word1);
    put((w0 & 0xFF000000) | (uint32_t(x1 * 4) << 12) | uint32_t(y1 * 4),
        (w1 & 0xFF000000) | (uint32_t(x0 * 4) << 12) | uint32_t(y0 * 4));
    put(rd_w(rect + 8) & 0xFF000000, (uint32_t(int(std::lround(s0 * 32))) << 16) | uint32_t(y0 << 5));
    put(rd_w(rect + 16), (uint32_t(int(std::lround(1024 / wide))) << 16) | 0x0400);
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    put(c[0].values.word0, c[0].values.word1);
    gEXSetRDRAMExtended(c, 0);
    put(c[0].values.word0, c[0].values.word1);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (rect + 24) & 0x1FFFFFFF);
    wr_w(rect, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(rect + 4, buffer & 0x1FFFFFFF);
    g_tiles = true;
    return rect + 24;
}

// How much a screen is scaled, false if not at all: until it fills the frame's height, in any window. With Aspect Ratio
// Original, not at all.
bool set_fill() {
    if (!mg_aspect_expand()) {
        return false;
    }
    g_fill = SCREEN_HEIGHT / INNER_HEIGHT;
    g_wide = mg_wide_scale() > 1.0f;
    return true;
}

// The commands from list to end that draw to the screen scaled: rectangles, scissors and viewports, rectangles point
// sampled.
void fill(uint32_t list, uint32_t end) {
    uint32_t texture = 0;
    bool copy = false;
    bool screen = true;
    for (uint32_t addr = list; addr < end; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        if (op == G_SETTIMG) {
            texture = w1 & 0x1FFFFFFF;
            if (screen && w0 == 0xFD10012F && texture == GAME_COPY && g_cimg != 0 &&
                (g_wipe_image != 0 || g_last_cimg != 0)) {
                uint32_t past = wipe_tile(addr, end);
                if (past != 0) {
                    addr = past - 8;
                }
            }
        }
        else if (w0 == SET_CYCLE_TYPE) {
            copy = (w1 == G_CYC_COPY);
        }
        else if (op == G_SETCIMG) {
            screen = (w0 & 0xFFF) + 1 == uint32_t(SCREEN_WIDTH);
            if (screen) {
                g_cimg = w1;
            }
        }
        else if (!screen) {
            if (op == G_TEXRECT && is_rect(addr)) {
                addr += 16;
            }
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

} // namespace

extern "C" void mg_screen_fill(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    // A wipe under way goes on through its second frame, in which the game swaps the two overlays and neither is loaded.
    bool wiping = g_wipe_image != 0;
    if ((list & 7) != 0 || !(loaded(MENU_OVERLAY) || loaded(TITLE_OVERLAY) || wiping) || !set_fill()) {
        g_wipe_image = 0;
        g_last_cimg = 0;
        return;
    }
    uint32_t end = list;
    for (uint32_t i = 0; i < MAX_COMMANDS && (rd_w(end) >> 24) != G_ENDDL; i++) {
        end += 8;
    }
    if ((rd_w(end) >> 24) != G_ENDDL) {
        return;
    }
    g_tiles = false;
    g_cimg = 0;
    fill(list, end);
    if (!g_tiles) {
        g_wipe_image = 0;
    }
    g_last_cimg = g_cimg;
}

// The commands from list to end, a 2D screen another file found in a frame that isn't all one (credits.cpp's), scaled
// the same way.
extern "C" void mg_screen_fill_range(uint8_t* rdram, uint32_t list, uint32_t end) {
    g_rdram = rdram;
    if (set_fill()) {
        fill(list, end);
    }
}

// Whether mg_screen_fill_range would scale a screen now.
extern "C" bool mg_screen_fills() {
    return set_fill();
}
