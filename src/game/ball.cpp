// The ball, the stars around the cup and the sun's lens flare past the 4:3 frame's sides (RT64's Aspect Ratio Expand):
// the ball, its shadow, its trail, the ring of stars around the cup and the flare's circles go on into the wide
// picture's sides with the course, rather than being cut along a vertical line at the 4:3 frame's side. And the ball's
// sprites where the ball is, to a quarter of a pixel, not the whole pixel the game rounds it to (the last paragraph).
//
// Up close (the view of the cup the game cuts to as a putt reaches it, with no HUD) the ball is not a model: it is
// texture rectangles drawn where the game works out the ball is on the screen, a sprite of the ball, of its shadow and
// of each piece of its trail, scaled with the distance (only its printed logo is 3D, quads around the ball's centre).
// The stars that ring the cup as the ball nears it are texture rectangles too, and so is each of the eight circles of
// the sun's lens flare, which the game draws over the course while the sun is in the 4:3 frame, on the line from the
// sun through the screen's middle across, 96 pixels down (a table at 0x800C6920 gives each its place on that line,
// its size and its colour), so that as the sun nears a side some of them lie past the other one. A rectangle can't
// start left of the screen, so where one would, the game starts it at 0 and its texture as far in as it cut (and one
// wholly past the left side at 0 too, 0 wide); the RDP's scissor, the 3D's (8 to 312), then cuts what lies between 0
// and 8, and on the right what lies past 312. RT64 widens the 3D to the picture but draws a rectangle where the game
// puts it, in the 4:3 frame, under that scissor in the 4:3 frame too, so in a wider window the ball, the stars and the
// flare were cut along a vertical line 8 pixels inside the 4:3 frame's side, the course drawn on past it.
//
// func_ovl0_801FE370 draws the ball's; func_8002DC10, the frame's drawing, calls it from two places (one per kind of
// frame it draws), each only while the ball is shown, func_ovl0_80217164, which draws the stars, from one, and
// func_8008D3F4, which draws the flare, right after it; overlay 7 calls the flare too, with the display list's head in
// a copy on its stack while it draws. They are asm in the decomp, so as with the HUD (src/game/hud.cpp) and the sun's
// light (src/game/backdrop.cpp) their commands are rewritten rather than the functions copied. us.toml hooks the calls
// on both sides (the stars' and the flare's as one): mg_ball_begin notes where the display list's head stands, and
// mg_ball_end reads what was added since, from the word its caller keeps the head in. A rectangle within the 3D's
// border band (left of 8 or right of 312), or cut at a side, is put back as the game had it before the cut: on the left
// its texture starts at the tile's own start again, the rectangle as far past the side as that takes (one cut to
// nothing as wide as its tile); on the right it goes on to the texture's end. It is drawn as RT64's extended texture
// rectangle stretched to the picture (G_EX_ASPECT_STRETCH), as RT64 draws one across the whole screen, so the game's
// own scissor, the 3D's, clips it where the wide picture's 3D ends; its place and texture step are squeezed toward the
// screen's middle by as much, so it keeps its shape and its place on the course. Those commands go to a buffer of this
// file's in RDRAM above the 4 MB the game uses, the rectangle's first command replaced by a branch to it, and its end a
// branch back. A rectangle well inside the 4:3 frame is left as it is, and with Aspect Ratio Original, or a window no
// wider than 4:3, nothing changes. The flare still shows only while the sun is in the 4:3 frame, as the game decides.
//
// With the General tab's Enhancements On (src/game/config.cpp), the flare's circles also fade out over the last 32
// pixels before the line where the game stops drawing them, 16 pixels inside each side of the screen, rather than all
// going in one frame (fade_flare() says how).
//
// The ball's sprites are also put where the ball is on the screen, across to a quarter of a pixel and down to the
// nearest whole one (its far dot of colour, which samples no texture, down to the quarter too), rather than on the
// pixel the game truncates it to. The game works out where on the screen the ball
// is, and its shadow, with func_80065650, which truncates x and y to whole pixels (its out[0] and out[1]), and
// func_ovl0_801FE370 draws each rectangle around that pixel: the ball's sprite near or its dots far, its shadow, and
// the coloured marker over it (overlay 7, the intro's, draws its ball with its own two of the same kind). Every console
// pixel is several of a modern window's, and RT64 draws the course under them at its full resolution and moves it
// smoothly, so the sprites stepped over the grass a whole console pixel at a time as the camera drifted, and wiggled at
// a high display rate. us.toml hooks func_80065650 where it truncates x and y, and while the ball is drawn
// (mg_ball_begin to mg_ball_end) mg_ball_project_x and _y note each point and what was dropped; mg_ball_end then moves
// a rectangle whose middle across is a noted point's x by that fraction (find_move() says why down is to a whole
// pixel), and a sprite the game draws in two halves, one over the other (the intro's ball), moves whole, its other half
// with the one the point is in (nudge_all() says why). It keeps the game's size, so RT64
// (RT64_INTERPOLATE_RECT_SCROLLS, CMakeLists.txt) still pairs it with the last frame's rectangle and slides it between
// the two frames at the display's rate, a texture rectangle across at the quarter pixel it is given
// (runtime-patches/rt64-1-textured-rects-across-to-a-quarter-pixel.patch). This happens whatever the window's shape, and before a rectangle at a side is
// widened. The game still rounds a sprite's size to whole pixels, so a frame in which the ball's sprite grows or
// shrinks by one isn't slid. mg_ball_project_x also keeps what was dropped of the last x, whoever asked for it, for
// src/game/aim.cpp, which moves the aim's marks by it the same way.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"
#include "mg.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETTILE = 0xF5, G_SETTILESIZE = 0xF2, G_SETCOMBINE = 0xFC;
constexpr uint32_t G_DL_NOPUSH = 1, G_TX_MIRROR = 1;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The screen, and the 3D's scissor within it, in quarter pixels.
constexpr int SCREEN_WIDTH = 320 * 4;
constexpr int SCREEN_HEIGHT = 240 * 4;
constexpr int SCISSOR_LEFT = 8 * 4;
constexpr int SCISSOR_RIGHT = 312 * 4;

// Above src/game/screen.cpp's buffers. Two calls a frame, and the renderer may read one a frame or more after the game
// sends it.
constexpr uint32_t BUFFER_BASE = 0x80770000;
constexpr uint32_t BUFFER_SIZE = 0x40;
constexpr uint32_t BUFFER_COUNT = 1024;
// The commands read from the game, at most.
constexpr uint32_t MAX_COMMANDS = 20000;
// Rectangles rewritten in one call, at most (the ball, its shadow and its trail, or the stars and the flare).
constexpr int MAX_RECTS = 64;

// The point on the screen func_80065650 works out, before it adds the screen's middle (the decomp's data at 0x800E1738
// and 0x800E173C, in pixels).
constexpr uint32_t MG_SCREEN_MIDDLE_X = 0x800E1738;
constexpr uint32_t MG_SCREEN_MIDDLE_Y = 0x800E173C;
// Points noted while the ball is drawn, at most (the ball and its shadow, and the far balls' and their shadows').
constexpr int MAX_SPOTS = 1024;
// Rectangles in the ball's list looked at for a move, at most.
constexpr int MAX_NUDGED = 1024;

// The sun's place on the screen, as floats (func_8008FF20 works them out; src/game/backdrop.cpp), which the flare reads.
// The flare's eight circles (its table at 0x800C6920, 0x14 bytes each, the float at 0x10 where on the line from the sun
// to the point it runs through each lies), drawn only while the sun is more than 16 pixels inside the screen. That
// point is the screen's middle across and 96 pixels down, not 120 (func_8008D3F4's constants).
constexpr uint32_t SUN_X = 0x800C5F24;
constexpr uint32_t SUN_Y = 0x800C5F2C;
constexpr uint32_t FLARE_TABLE = 0x800C6920;
constexpr uint32_t FLARE_STRIDE = 0x14;
constexpr int FLARE_CIRCLES = 8;
constexpr int FLARE_MAX_HALF = 255;
constexpr float FLARE_MIDDLE_X = 160.0f;
constexpr float FLARE_MIDDLE_Y = 96.0f;
// The game draws the flare while the sun is more than FLARE_EDGE pixels inside each side of the screen. With
// Enhancements On its circles fade out over the last FLARE_FADE pixels before that, rather than going at once.
constexpr float FLARE_EDGE = 16.0f;
constexpr float FLARE_FADE = 32.0f;
constexpr uint32_t G_SETPRIMCOLOR = 0xFA;

uint8_t* g_rdram;
uint32_t g_start;
bool g_open = false;
uint32_t g_next_buffer = 0;

// A point func_80065650 truncated: the whole pixel it gives (with the screen's middle added) and what it dropped, a
// fraction of a pixel either side.
struct Spot {
    int x, y;
    float dx, dy;
};
Spot g_spots[MAX_SPOTS];
int g_spot_count = 0;
bool g_have_x = false;
int g_x;
float g_dx;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

struct Writer {
    uint32_t pos, end;
    bool ok = true;
    void put(uint32_t w0, uint32_t w1) {
        if (pos + 8 > end) {
            ok = false;
            return;
        }
        wr_w(pos, w0);
        wr_w(pos + 4, w1);
        pos += 8;
    }
    void put(const GfxCommand* cmds, int n) {
        for (int i = 0; i < n; i++) {
            put(cmds[i].values.word0, cmds[i].values.word1);
        }
    }
};

// What the last G_SETTILE and G_SETTILESIZE said of a tile.
struct Tile {
    int uls = 0, lrs = 0; // in texels
    bool mirror = false;
};

// The rectangle at addr (its three commands) put back past the side of the screen it was cut at and drawn into the
// wide picture's sides, in a buffer that branches back after it. False if it lies well inside the 4:3 frame, or didn't
// fit.
bool widen(uint32_t addr, const Tile& tile, float scale) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF); // quarter pixels
    int lrx = int((w0 >> 12) & 0xFFF), lry = int(w0 & 0xFFF);
    int s = int16_t(st >> 16), t = int16_t(st & 0xFFFF); // s10.5
    int dsdx = int16_t(d >> 16), dtdy = int16_t(d & 0xFFFF); // s5.10
    if (dsdx <= 0 || uly >= lry || lry <= 0 || uly >= SCREEN_HEIGHT) {
        return false;
    }
    int start = tile.uls * 32;
    int end = (tile.uls + (tile.lrs - tile.uls + 1) * (tile.mirror ? 2 : 1)) * 32;
    int new_ulx = ulx, new_lrx = lrx, new_s = s;
    // s moves by dsdx / 128 a quarter pixel.
    if (ulx == 0 && s > start) {
        new_ulx = -((s - start) * 128 / dsdx);
        new_s = start;
        if (lrx == 0) {
            new_lrx = new_ulx + (end - start) * 128 / dsdx;
        }
    }
    if (lrx == SCREEN_WIDTH && ulx < lrx) {
        int s_right = s + (lrx - ulx) * dsdx / 128;
        if (end > s_right) {
            new_lrx = lrx + (end - s_right) * 128 / dsdx;
        }
    }
    if (new_lrx <= new_ulx || (new_ulx >= SCISSOR_LEFT && new_lrx <= SCISSOR_RIGHT)) {
        return false;
    }

    // Drawn stretched to the picture, as RT64 draws a rectangle across the whole screen: the game's own scissor (the
    // 3D's, 8 to 312) is then the 3D's in the picture too. So that the ball keeps its shape and its place, the rectangle
    // is put nearer the screen's middle and its texture stepped faster by as much as the stretch spreads them.
    auto squeeze = [scale](int x) { return int(std::lround(SCREEN_WIDTH / 2 + (x - SCREEN_WIDTH / 2) / scale)); };
    int wide_dsdx = int(std::lround(dsdx * scale));
    if (wide_dsdx > INT16_MAX) {
        return false;
    }

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[3];
    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    gEXTextureRectangle(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, squeeze(new_ulx), uly, squeeze(new_lrx), lry,
                        (w1 >> 24) & 7, new_s & 0xFFFF, t & 0xFFFF, wide_dsdx & 0xFFFF, dtdy & 0xFFFF);
    out.put(c, 3);
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    out.put(c, 1);
    out.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), (addr + 24) & 0x1FFFFFFF);
    if (!out.ok) {
        return false;
    }
    wr_w(addr, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
    return true;
}

// A texture rectangle in the ball's list: where it is (quarter pixels), and the move nudge() gives it.
struct Rect {
    uint32_t addr;
    bool textured;
    int ulx, uly, lrx, lry;
    bool found = false;
    int dx = 0, dy = 0;
};

// The move for a rectangle drawn around a noted point: its middle across on the point's x, and the point's y within it,
// the nearest to its middle down if more than one (the ball is as tall as it is wide, its shadow flatter). To the
// quarter pixel, but down to the whole pixel nearest for one that samples a texture. RT64 draws a texture rectangle
// from the whole pixel its top rounds up to, and one whose top isn't on a whole pixel with its texture a row further
// on, so a top moved by a fraction moved the sprite a whole pixel down and lost its picture's top row (the ball looked
// shaved off at the top), and with t moved back by that row, a sprite whose texture's rows are finer than the screen's
// lost its bottom in the frames between the game's instead. One that samples none (the far ball's dot of colour) RT64
// draws at the corners it is given. False if it is no such rectangle, or is at or past a side of the screen (left for
// widen()).
bool find_move(Rect& r) {
    if (r.ulx <= 0 || r.lrx >= SCREEN_WIDTH || r.lrx <= r.ulx || r.uly <= 0 || r.lry >= SCREEN_HEIGHT ||
        ((r.ulx + r.lrx) & 1) != 0) {
        return false;
    }
    int mid_x = (r.ulx + r.lrx) / 2, mid_y = r.uly + (r.lrx - r.ulx) / 2;
    const Spot* best = nullptr;
    for (int i = 0; i < g_spot_count; i++) {
        const Spot& spot = g_spots[i];
        if (spot.x * 4 != mid_x || spot.y * 4 < r.uly - 4 || spot.y * 4 > std::max(r.lry, mid_y) + 4) {
            continue;
        }
        if (best == nullptr || std::abs(spot.y * 4 - mid_y) < std::abs(best->y * 4 - mid_y)) {
            best = &spot;
        }
    }
    if (best == nullptr) {
        return false;
    }
    r.dx = int(std::lround(best->dx * 4));
    r.dy = r.textured ? int(std::lround(best->dy)) * 4 : int(std::lround(best->dy * 4));
    return true;
}

// Whether the combiner G_SETCOMBINE's words set reads a texel, in either cycle: TEXEL0 or TEXEL1 (1 or 2) in any of its
// inputs, or their alpha (8 or 9) as the colour's multiplier (the decomp's include/PR/gbi.h).
bool combine_textured(uint32_t w0, uint32_t w1) {
    const int color_ab[] = { int((w0 >> 20) & 0xF), int((w1 >> 28) & 0xF), int((w0 >> 5) & 0xF), int((w1 >> 24) & 0xF) };
    const int color_c[] = { int((w0 >> 15) & 0x1F), int(w0 & 0x1F) };
    const int seven[] = { int((w1 >> 15) & 7), int((w1 >> 6) & 7),                           // the colour's d
                          int((w0 >> 12) & 7), int((w1 >> 12) & 7), int((w0 >> 9) & 7), int((w1 >> 9) & 7), // alpha
                          int((w1 >> 21) & 7), int((w1 >> 3) & 7), int((w1 >> 18) & 7), int(w1 & 7) };
    for (int v : color_ab) {
        if (v == 1 || v == 2) {
            return true;
        }
    }
    for (int v : color_c) {
        if (v == 1 || v == 2 || v == 8 || v == 9) {
            return true;
        }
    }
    for (int v : seven) {
        if (v == 1 || v == 2) {
            return true;
        }
    }
    return false;
}

// Whether a and b are the two halves of one sprite, the game's way of drawing a texture too tall to load at once: as
// wide as each other, one right under the other.
bool halves(const Rect& a, const Rect& b) {
    return a.ulx == b.ulx && a.lrx == b.lrx && (a.lry == b.uly || b.lry == a.uly);
}

// The rectangle moved by its move. It keeps the game's size, so RT64 still pairs it with the last frame's and slides it
// between the two.
void nudge(const Rect& r) {
    int dx = r.dx, dy = r.dy;
    if ((dx == 0 && dy == 0) || r.ulx + dx <= 0 || r.lrx + dx >= SCREEN_WIDTH || r.uly + dy <= 0 ||
        r.lry + dy >= SCREEN_HEIGHT) {
        return;
    }
    uint32_t w0 = rd_w(r.addr), w1 = rd_w(r.addr + 4);
    wr_w(r.addr, (w0 & 0xFF000000) | (uint32_t(r.lrx + dx) << 12) | uint32_t(r.lry + dy));
    wr_w(r.addr + 4, (w1 & 0xFF000000) | (uint32_t(r.ulx + dx) << 12) | uint32_t(r.uly + dy));
}

// Each rectangle drawn around a noted point moved by what the point lost. A sprite drawn in two
// halves (the intro's ball, 8 pixels by two of 4) has its point in one half only, at its foot, and the other half
// moves with it, or the sprite came apart by up to a pixel, the top half where the game put it and the bottom half
// moved. Two rectangles that each have a point of their own (the ball and its shadow under it) each move by their own.
void nudge_all(uint32_t start, uint32_t end) {
    if (g_spot_count == 0 || end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }
    static Rect rects[MAX_NUDGED];
    int count = 0;
    // Whether the combiner the list last set reads a texel; one that sets none is taken to.
    bool textured = true;
    for (uint32_t addr = start; addr + 8 <= end && count < MAX_NUDGED; addr += 8) {
        uint32_t w0 = rd_w(addr), op = w0 >> 24;
        if (op == G_SETCOMBINE) {
            textured = combine_textured(w0, rd_w(addr + 4));
        } else if (op == G_TEXRECT && addr + 24 <= end && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
                   (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
            uint32_t w1 = rd_w(addr + 4);
            Rect& r = rects[count++];
            r = Rect{ addr, textured, int((w1 >> 12) & 0xFFF), int(w1 & 0xFFF), int((w0 >> 12) & 0xFFF), int(w0 & 0xFFF) };
            r.found = find_move(r);
            addr += 16;
        }
    }
    for (int i = 0; i < count; i++) {
        Rect& r = rects[i];
        if (r.found) {
            continue;
        }
        const Rect* other = nullptr;
        if (i > 0 && rects[i - 1].found && halves(rects[i - 1], r)) {
            other = &rects[i - 1];
        } else if (i + 1 < count && rects[i + 1].found && halves(r, rects[i + 1])) {
            other = &rects[i + 1];
        }
        if (other != nullptr) {
            r.dx = other->dx;
            r.dy = other->dy;
        }
    }
    for (int i = 0; i < count; i++) {
        nudge(rects[i]);
    }
}

float rd_f(uint32_t addr) {
    uint32_t w = rd_w(addr);
    float f;
    std::memcpy(&f, &w, sizeof f);
    return f;
}

// With Enhancements On, the flare's circles fade out as the sun nears the line where the game stops drawing them,
// FLARE_EDGE pixels inside each side of the screen, over the last FLARE_FADE pixels before it, rather than all going
// in one frame. func_8008D3F4 puts circle i at the sun's place plus (FLARE_MIDDLE less it) times the table's float, as
// floats, and draws it as a square from that less its half size to that plus it, each truncated and at 0 at least,
// over a texture stepped 0x10000 / half (s5.10), after a G_SETPRIMCOLOR whose alpha is the flare's brightness. A
// rectangle in the list that is just that square for one of the circles has the alpha of the G_SETPRIMCOLOR before it
// scaled by how far the sun is into the fade; nothing else in the list is touched. The circles are left where the game
// puts them, on whole pixels: RT64 pairs each with the last frame's and draws it between there and here in the frames
// between, which moves them evenly, where moving their texture within them by the fraction of a pixel the game drops
// made them lag in those frames and catch up on the game's own.
void fade_flare(uint32_t start, uint32_t end) {
    if (end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }
    float sx = rd_f(SUN_X), sy = rd_f(SUN_Y);
    float inside = std::min(std::min(sx, SCREEN_WIDTH / 4 - sx), std::min(sy, SCREEN_HEIGHT / 4 - sy)) - FLARE_EDGE;
    if (!(inside > 0.0f)) {
        return;
    }
    float fade = std::min(1.0f, inside / FLARE_FADE);
    if (!mg::enhancements() || !(fade < 1.0f)) {
        return;
    }
    float cx[FLARE_CIRCLES], cy[FLARE_CIRCLES];
    for (int i = 0; i < FLARE_CIRCLES; i++) {
        float f = rd_f(FLARE_TABLE + FLARE_STRIDE * i + 0x10);
        cx[i] = sx + (FLARE_MIDDLE_X - sx) * f;
        cy[i] = sy + (FLARE_MIDDLE_Y - sy) * f;
    }
    // The game's edge: truncated, then at 0 at least, in quarter pixels.
    auto edge = [](float v) { return std::max(0, int(v)) * 4; };
    // The last G_SETPRIMCOLOR, whose alpha the game sets to the flare's brightness before each circle.
    uint32_t prim = 0;
    for (uint32_t addr = start; addr + 24 <= end; addr += 8) {
        if ((rd_w(addr) >> 24) == G_SETPRIMCOLOR) {
            prim = addr;
            continue;
        }
        if ((rd_w(addr) >> 24) != G_TEXRECT || (rd_w(addr + 8) >> 24) != G_RDPHALF_1 ||
            (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
            continue;
        }
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), d = rd_w(addr + 20);
        int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF);
        int lrx = int((w0 >> 12) & 0xFFF), lry = int(w0 & 0xFFF);
        int dsdx = int16_t(d >> 16), dtdy = int16_t(d & 0xFFFF);
        addr += 16;
        if (dsdx <= 0 || dsdx != dtdy) {
            continue;
        }
        for (int i = 0; i < FLARE_CIRCLES; i++) {
            int half = 0x10000 / dsdx;
            // 0x10000 / half is dsdx for a half or two about 0x10000 / dsdx.
            for (int h = std::max(1, half - 1); h <= std::min(FLARE_MAX_HALF, half + 1); h++) {
                if (0x10000 / h != dsdx || ulx != edge(cx[i] - h) || lrx != edge(cx[i] + h) || uly != edge(cy[i] - h) ||
                    lry != edge(cy[i] + h)) {
                    continue;
                }
                if (prim != 0) {
                    uint32_t color = rd_w(prim + 4);
                    wr_w(prim + 4, (color & ~0xFFu) | uint32_t(std::lround((color & 0xFF) * fade)));
                }
                i = FLARE_CIRCLES;
                break;
            }
        }
    }
}

void widen_all(uint32_t start, uint32_t end) {
    if (end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }
    float scale = mg_wide_scale();
    if (scale <= 1.0f) {
        return;
    }
    Tile tiles[8];
    int done = 0;
    for (uint32_t addr = start; addr + 8 <= end && done < MAX_RECTS; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        switch (w0 >> 24) {
            case G_SETTILE:
                tiles[(w1 >> 24) & 7].mirror = ((w1 >> 8) & G_TX_MIRROR) != 0;
                break;
            case G_SETTILESIZE: {
                Tile& tile = tiles[(w1 >> 24) & 7];
                tile.uls = int((w0 >> 12) & 0xFFF) / 4;
                tile.lrs = int((w1 >> 12) & 0xFFF) / 4;
                break;
            }
            case G_TEXRECT:
                if (addr + 24 <= end && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 && (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
                    if (widen(addr, tiles[(w1 >> 24) & 7], scale)) {
                        done++;
                    }
                    addr += 16;
                }
                break;
        }
    }
}

} // namespace

// head: the word the caller keeps the display list's head in, MG_GFX_HEAD or a copy of it (as mg_ball_end's).
extern "C" void mg_ball_begin(uint8_t* rdram, uint32_t head) {
    g_rdram = rdram;
    g_start = rd_w(head);
    g_open = true;
    g_spot_count = 0;
    g_have_x = false;
}

// What func_80065650 dropped of the last x it truncated, whoever called it: src/game/aim.cpp moves the aim's marks by
// it, as mg_ball_end moves the ball's sprites.
static float g_last_dx;

float mg_last_projected_dx() {
    return g_last_dx;
}

// value: the point's x or y on the screen before func_80065650 truncates it and adds the screen's middle. y follows x.
extern "C" void mg_ball_project_x(uint8_t* rdram, float value) {
    g_last_dx = std::isfinite(value) ? value - std::trunc(value) : 0.0f;
    if (!g_open) {
        return;
    }
    g_rdram = rdram;
    float whole = std::trunc(value);
    g_x = int(whole) + int32_t(rd_w(MG_SCREEN_MIDDLE_X));
    g_dx = value - whole;
    g_have_x = true;
}

extern "C" void mg_ball_project_y(uint8_t* rdram, float value) {
    if (!g_open || !g_have_x) {
        return;
    }
    g_have_x = false;
    if (g_spot_count >= MAX_SPOTS || !std::isfinite(value) || !std::isfinite(g_dx)) {
        return;
    }
    g_rdram = rdram;
    float whole = std::trunc(value);
    g_spots[g_spot_count++] = { g_x, int(whole) + int32_t(rd_w(MG_SCREEN_MIDDLE_Y)), g_dx, value - whole };
}

// head: the word the caller keeps the display list's head in, the same as mg_ball_begin's.
extern "C" void mg_ball_end(uint8_t* rdram, uint32_t head) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    uint32_t end = rd_w(head);
    fade_flare(g_start, end);
    nudge_all(g_start, end);
    widen_all(g_start, end);
}
