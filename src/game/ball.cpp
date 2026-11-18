// The ball and the stars around the cup past the 4:3 frame's sides (RT64's Aspect Ratio Expand): the ball, its shadow,
// its trail and the ring of stars around the cup go on into the wide picture's sides with the course, rather than being
// cut along a vertical line at the 4:3 frame's side.
//
// Up close (the view of the cup the game cuts to as a putt reaches it, with no HUD) the ball is not a model: it is
// texture rectangles drawn where the game works out the ball is on the screen, a sprite of the ball, of its shadow and
// of each piece of its trail, scaled with the distance (only its printed logo is 3D, quads around the ball's centre).
// The stars that ring the cup as the ball nears it are texture rectangles too. A rectangle can't start left of the
// screen, so where one would, the game starts it at 0 and its texture as far in as it cut (and one wholly past the left
// side at 0 too, 0 wide); the RDP's scissor, the 3D's (8 to 312), then cuts what lies between 0 and 8. RT64 widens the
// 3D to the picture but draws a rectangle where the game puts it, in the 4:3 frame, under that scissor in the 4:3 frame
// too, so in a wider window the ball and the stars were cut along a vertical line 8 pixels inside the 4:3 frame's side,
// the course drawn on past it.
//
// func_ovl0_801FE370 draws the ball's; func_8002DC10, the frame's drawing, calls it from two places (one per kind of
// frame it draws), each only while the ball is shown, and func_ovl0_80217164, which draws the stars, from one. They are
// asm in the decomp, so as with the HUD (src/game/hud.cpp) and the sun's light (src/game/backdrop.cpp) their commands
// are rewritten rather than the functions copied. us.toml hooks the three calls on both sides: mg_ball_begin notes
// where the display list's head stands, and mg_ball_end reads what was added since. A rectangle within the 3D's border
// band (left of 8 or right of 312), or cut at a side, is put back as the game had it before the cut: on the left its
// texture starts at the tile's own start again, the rectangle as far past the side as that takes (one cut to nothing as
// wide as its tile); on the right it goes on to the texture's end. It is drawn as RT64's extended texture rectangle
// stretched to the picture (G_EX_ASPECT_STRETCH), as RT64 draws one across the whole screen, so the game's own scissor,
// the 3D's, clips it where the wide picture's 3D ends; its place and texture step are squeezed toward the screen's
// middle by as much, so it keeps its shape and its place on the course. Those commands go to a buffer of this file's in
// RDRAM above the 4 MB the game uses, the rectangle's first command replaced by a branch to it, and its end a branch
// back. A rectangle well inside the 4:3 frame is left as it is, and with Aspect Ratio Original, or a window no wider
// than 4:3, nothing changes.
//
// RT64 slides 3D between the game's frames to the display's rate (Framerate Display) but draws a rectangle where the
// game last put it, so as a fast ball rolls its 3D logo can sit a little off the sprite between frames, as it does
// without this file; at the game's own rate it stays on the ball.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETTILE = 0xF5, G_SETTILESIZE = 0xF2;
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
// Rectangles rewritten in one call, at most (the ball, its shadow and its trail, or the stars).
constexpr int MAX_RECTS = 64;

uint8_t* g_rdram;
uint32_t g_start;
bool g_open = false;
uint32_t g_next_buffer = 0;

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

extern "C" void mg_ball_begin(uint8_t* rdram) {
    g_rdram = rdram;
    g_start = rd_w(MG_GFX_HEAD);
    g_open = true;
}

extern "C" void mg_ball_end(uint8_t* rdram) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    widen_all(g_start, rd_w(MG_GFX_HEAD));
}
