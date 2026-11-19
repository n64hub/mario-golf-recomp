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
// sun through the screen's middle (a table at 0x800C6920 gives each its place on that line, its size and its colour),
// so that as the sun nears a side some of them lie past the other one. A rectangle can't start left of the screen, so
// where one would, the game starts it at 0 and its texture as far in as it cut (and one wholly past the left side at 0
// too, 0 wide); the RDP's scissor, the 3D's (8 to 312), then cuts what lies between 0 and 8, and on the right what lies
// past 312. RT64 widens the 3D to the picture but draws a rectangle where the game puts it, in the 4:3 frame, under that
// scissor in the 4:3 frame too, so in a wider window the ball, the stars and the flare were cut along a vertical line 8
// pixels inside the 4:3 frame's side, the course drawn on past it.
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
// The ball's sprites are also put where the ball is on the screen, to a quarter of a pixel, rather than on the whole
// pixel the game rounds it to. The game works out where on the screen the ball is, and its shadow, with func_80065650,
// which truncates x and y to whole pixels (its out[0] and out[1]), and func_ovl0_801FE370 draws each rectangle around
// that pixel: the ball's sprite near or its dots far, its shadow, and the coloured marker over it (overlay 7, the
// intro's, draws its ball with its own two of the same kind). Every console pixel
// is several of a modern window's, and RT64 draws the course under them at its full resolution and moves it smoothly,
// so the sprites stepped over the grass a whole console pixel at a time as the camera drifted, and wiggled at a high
// display rate. us.toml hooks func_80065650 where it truncates x and y, and while the ball is drawn
// (mg_ball_begin to mg_ball_end) mg_ball_project_x and _y note each point and what was dropped; mg_ball_end then moves
// a rectangle whose middle across is a noted point's x by that fraction, rounded to the quarter pixel a rectangle's
// corners take. It keeps the game's size, so RT64 (RT64_INTERPOLATE_RECT_SCROLLS, CMakeLists.txt) still pairs it with
// the last frame's rectangle and slides it between the two frames at the display's rate. This happens whatever the
// window's shape, and before a rectangle at a side is widened. The game still rounds a sprite's size to whole pixels,
// so a frame in which the ball's sprite grows or shrinks by one isn't slid.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETTILE = 0xF5, G_SETTILESIZE = 0xF2;
constexpr uint32_t G_SETOTHERMODE_H = 0xE3, G_RDPSETOTHERMODE = 0xEF;
constexpr uint32_t G_DL_NOPUSH = 1, G_TX_MIRROR = 1;
constexpr int G_MDSFT_CYCLETYPE = 20;
constexpr uint32_t G_CYCLE_COPY = 2;

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

// The rectangle at addr moved by the fraction of a pixel the point it is drawn around lost, if it is one: its middle
// across on a noted point's x, and that point's y within it, the nearest to its middle down if more than one (the ball
// is as tall as it is wide, its shadow flatter). It keeps the game's size, so RT64 still pairs it with the last frame's
// and slides it between the two. One at or past a side of the screen is left for widen().
//
// Its texture moves with it whole. RT64 draws a rectangle whose top isn't on a whole pixel with its texture a row
// further on, as the RDP starts it on the next scanline with the next row, but at a high resolution it still draws
// that rectangle from its top, so the sprite's top row went missing whenever the ball's top fell between pixels, and
// the ball flicked between cut and whole as it moved (RT64 then also slid the texture between the frames). So t is
// moved back by the row RT64 adds when the top is moved off a whole pixel (forward when moved onto one). copy: the RDP's
// copy mode, in which RT64 puts the top on a whole pixel and adds no row.
void nudge(uint32_t addr, bool copy) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF); // quarter pixels
    int lrx = int((w0 >> 12) & 0xFFF), lry = int(w0 & 0xFFF);
    if (ulx <= 0 || lrx >= SCREEN_WIDTH || lrx <= ulx || uly <= 0 || lry >= SCREEN_HEIGHT || ((ulx + lrx) & 1) != 0) {
        return;
    }
    int mid_x = (ulx + lrx) / 2, mid_y = uly + (lrx - ulx) / 2;
    const Spot* best = nullptr;
    for (int i = 0; i < g_spot_count; i++) {
        const Spot& spot = g_spots[i];
        if (spot.x * 4 != mid_x || spot.y * 4 < uly - 4 || spot.y * 4 > std::max(lry, mid_y) + 4) {
            continue;
        }
        if (best == nullptr || std::abs(spot.y * 4 - mid_y) < std::abs(best->y * 4 - mid_y)) {
            best = &spot;
        }
    }
    if (best == nullptr) {
        return;
    }
    int dx = int(std::lround(best->dx * 4)), dy = int(std::lround(best->dy * 4));
    if ((dx == 0 && dy == 0) || ulx + dx <= 0 || lrx + dx >= SCREEN_WIDTH || uly + dy <= 0 || lry + dy >= SCREEN_HEIGHT) {
        return;
    }
    wr_w(addr, (w0 & 0xFF000000) | (uint32_t(lrx + dx) << 12) | uint32_t(lry + dy));
    wr_w(addr + 4, (w1 & 0xFF000000) | (uint32_t(ulx + dx) << 12) | uint32_t(uly + dy));
    if (!copy && ((uly & 3) != 0) != (((uly + dy) & 3) != 0)) {
        uint32_t st = rd_w(addr + 12);
        int t = int16_t(st & 0xFFFF);                      // s10.5
        int row = int16_t(rd_w(addr + 20) & 0xFFFF) >> 5; // dtdy (s5.10) in t's units, as RT64 adds it
        t += ((uly & 3) != 0) ? row : -row;
        wr_w(addr + 12, (st & 0xFFFF0000) | (uint32_t(t) & 0xFFFF));
    }
}

void nudge_all(uint32_t start, uint32_t end) {
    if (g_spot_count == 0 || end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }
    // The cycle type the list last set (G_SETOTHERMODE_H's or G_RDPSETOTHERMODE's bits 20-21); one or two cycles until
    // it sets one, as the ball's sprites are drawn scaled, which copy mode can't.
    uint32_t cycle = 0;
    for (uint32_t addr = start; addr + 8 <= end; addr += 8) {
        uint32_t w0 = rd_w(addr), op = w0 >> 24;
        if (op == G_SETOTHERMODE_H) {
            int len = int(w0 & 0xFF) + 1, shift = 32 - int((w0 >> 8) & 0xFF) - len;
            if (shift <= G_MDSFT_CYCLETYPE && shift + len >= G_MDSFT_CYCLETYPE + 2) {
                cycle = (rd_w(addr + 4) >> G_MDSFT_CYCLETYPE) & 3;
            }
        } else if (op == G_RDPSETOTHERMODE) {
            cycle = (w0 >> G_MDSFT_CYCLETYPE) & 3;
        } else if (op == G_TEXRECT && addr + 24 <= end && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
                   (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
            nudge(addr, cycle == G_CYCLE_COPY);
            addr += 16;
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

// value: the point's x or y on the screen before func_80065650 truncates it and adds the screen's middle. y follows x.
extern "C" void mg_ball_project_x(uint8_t* rdram, float value) {
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
    nudge_all(g_start, end);
    widen_all(g_start, end);
}
