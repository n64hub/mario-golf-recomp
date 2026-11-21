// The sun's light at the wide picture's sides (RT64's Aspect Ratio Expand): its beam and glare go on past the 4:3
// frame's side where the sun is, rather than ending in a vertical line there, and stay while any of it is in the
// picture.
//
// The sky behind the course has the sun's light drawn on it in screen space, where the sun is: a texture rectangle, a
// square around the sun as tall as it is wide (from the sun's x less its size plus one to its x plus its size, the size
// the game's sky's: 64 by day, 96 at sunset, and on Training's hole 10 it shrinks from 64 to 32 as the camera follows
// a drive), over a 64-texel texture whose s and t are mirrored past it, so a square 128 pixels across shows it and its
// mirror image and one 64 across it once, in a pale colour, before the course. func_8008FF20 draws it. Where the sun is near or past the screen's left
// side, the game cuts the rectangle at 0 and starts its texture as far in as it cut (wholly past it, the rectangle is
// cut to nothing); on the right it lets the rectangle run past 320. Either way the game's scissor, the 3D's (8 to 312),
// cuts it at the 4:3 frame's side. RT64 widens the 3D to the picture but draws a rectangle where the game puts it, in
// the 4:3 frame, under that scissor, so in a wider window the light was cut along a vertical line at that frame's side,
// with the plain sky beyond. And the game draws the light only while the sun is within 64 pixels of the screen, so with
// the sun further out the light went at once, though the wide picture still showed some of it.
//
// func_80091110 draws the sky with it (and the course's backdrop), twice a frame, from func_8002DC10, the frame's
// drawing; it is asm in the decomp, so as with the HUD (src/game/hud.cpp) its commands are rewritten rather than the
// function copied. us.toml hooks both calls on both sides: mg_backdrop_begin notes where the display list's head
// stands, and mg_backdrop_end reads what was added since. A rectangle that reaches past the scissor is drawn again past
// it, and one cut at the left side is put back as the game had it before the cut: its texture starts at the tile's own
// start again, the rectangle as far past the side as that takes, and one cut to nothing is put back as wide as the
// square is tall (its height the game's own, or as much more as it cut at the top, read from t as the cut on the left
// is from s). Run on to the mirrored texture's end there instead (128 texels), a sun of size 32 drew the texture's
// mirrored copy 64 pixels right of the sun, as a second sun. It is drawn as RT64's extended texture rectangle stretched
// to the picture (G_EX_ASPECT_STRETCH), as RT64 draws one across the whole screen, so the game's own scissor, the 3D's,
// clips it where the wide picture's 3D ends; its place and texture step are squeezed toward the screen's middle by as
// much, so the light keeps its shape. The light (the rectangle place_sun finds) is drawn so in every frame of a wide
// window, within the scissor too: drawn as the game's rectangle while within it and stretched once past it, the frame
// it crossed a side RT64 paired the one with the other as the same rectangle, and drew that game frame's frames between
// from the squeezed place, so the light's edge jumped tens of pixels for a frame and slid back, a flicker each time the
// camera panned it across a side of the 4:3 frame (issue 151). A scissor of its own, widened past 4:3 in the 4:3
// frame's coordinates, doesn't hold it: the light reaches the black border past the 3D, which nothing clears. Those
// commands go to a buffer of this file's in RDRAM above the 4 MB the game uses, the rectangle's first command replaced
// by a branch to it, and its end a branch back. func_8008FF20's bounds on the sun's x (-64 and 384) are read through
// mg_sun_bound_x, a us.toml hook that moves the x they compare toward the screen's middle by as much as the wide
// picture reaches past 4:3 and the light's half-width past the 64 those bounds allow for, so the light is drawn until
// the last of it leaves the picture. The game's sky table (0x800C6014: a row a course, an entry for each weather and
// time of day) gives each sky its sun, whose half-width is 96 at sunset: read as 64, that sun's light went at once
// with its last 32 pixels still inside the wide picture's side, and came in at once as wide. With Aspect Ratio Original,
// or a window no wider than 4:3, nothing changes. Nor does it where the 3D doesn't fill the screen (the end credits'
// character preview, a window that shrinks to the screen's right part over black, and the small view of where the ball
// lands), under a scissor of its own: in the credits the light widened there, and drawn for a sun the game would have
// left out, showed as an orange block on the black beside the window. So both read the scissor the game last set before
// the sky, and leave the light as the game draws it unless that is the 3D's whole one.
//
// The sky's clouds slide on every frame drawn as the camera turns, with the course in front of them, rather than a step
// a game frame. The sky is the first thing func_80091110 draws: three triangles, under a modelview matrix of their
// own, from ten vertices that never move (at 0x800C69C0 and 0x800C6A60 on alternate frames, as the display list
// alternates between two buffers), whose texture coordinates the game writes anew each frame as the camera turns, over
// a 64-texel cloud texture that wraps. RT64 slides a 3D draw's texture between the game's frames only for a matrix
// group that asks it to (its extended GBI's texcoord interpolation), and an untagged one asks nothing, so with
// interpolation on the clouds stepped at the game's 30 a second while the trees moved at the display's rate (the far
// hills, texture rectangles, are RT64_INTERPOLATE_RECT_SCROLLS's, CMakeLists.txt). The sky's matrix command is
// replaced by a branch to a buffer that opens such a group around it, and the command after its last triangle by one
// that closes it. The texture coordinates' wrap point is the cloud texture's own period, so where the game's
// coordinates wrap the clouds slide the short way, as they look, not across the sky in one frame. Whatever the aspect
// ratio.
//
// And the light is put where the sun is, to a thirty-second of a pixel, not on the whole pixel the game truncates it
// to. func_8008FF20 works out the sun's place on the screen as floats (at 0x800C5F24 and 0x800C5F2C, one of each per
// view), x from the camera's turn, and draws the light around their truncation. Every console pixel is several of a
// modern window's, and RT64 (RT64_INTERPOLATE_RECT_SCROLLS) slides the rectangle between where each game frame put
// it, so as the camera panned at a high display rate the sun and its glow went in uneven steps sideways, against the
// trees and hills moving smoothly in front. Moving the rectangle by a fraction of a pixel does nothing: RT64 puts a
// textured rectangle's edges on whole pixels. So mg_backdrop_end finds the light's rectangle by where the game puts
// it for a view's floats (its corners the truncated place less the size plus one and plus the size, each side at 0
// at least) and starts its texture earlier by what the truncation dropped, each way, before it is widened: the light
// is drawn that far right and down inside the rectangle, whose texture's edges are clear. RT64 pairs the rectangle
// with the last frame's as before and slides that step of its texture between them as it does a scroll. widen()
// puts a cut at the left back in whole pixels and keeps the fraction in s, and puts the squeezed rectangle's left
// edge on the whole pixel RT64 would, its texture starting as much further on, and its right edge the squeezed width
// on from there: rounded each its own way, the edges came 1.25 pixels apart in a game frame now and then, and RT64
// pairs a rectangle that changed size only while its edges moved a pixel apart at most, so that frame's light was
// drawn where the game put it, not slid there, a twitch as the camera panned. Whatever the window's shape.
//
// And the far hills keep the width they have in the 4:3 frame. func_80091110 draws them after the light, as strips of
// a panorama 512 texels across that repeats across s (its tile's mask), each 4 rows of it, a copy-mode texture
// rectangle across the 3D's whole width at a texel a pixel. border.cpp runs each on to the screen's sides, and RT64
// stretches a rectangle as wide as the screen across the window, so in a window wider than 4:3 the hills were wider
// than the course in front of them by as much, and where border.cpp stopped short of them (it read the light's
// extended rectangle as the end of the list; border.cpp's header) they drew at their own width inside the 4:3 frame,
// so as the camera turned past the sun they jumped between the two. Each strip is drawn instead from a buffer of this
// file's (STRIP_BASE), its rectangle the game's own but for its texture's step and start: stretched to the picture
// (G_EX_ASPECT_STRETCH, so the same with border.cpp or without it), its step as much faster as the picture is wider
// than 4:3 and its start as much earlier as the picture reaches past the 4:3 frame on the left, so each texel stays a
// 4:3 pixel wide and where the game puts it, the panorama going on at the sides. The start is kept within one repeat
// of the texture, which draws the same, so border.cpp can move it back further as it runs the strip to the screen's
// side. The light is drawn before the hills, so where they cover the sun it stays behind them, as in the 4:3 frame:
// it showed past that frame's sides only where the hills ended there.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETTILESIZE = 0xF2, G_SETTILE = 0xF5;
constexpr uint32_t G_DL_NOPUSH = 1;
constexpr uint32_t G_VTX = 0x01, G_TRI1 = 0x05, G_TRI2 = 0x06, G_QUAD = 0x07, G_MTX = 0xDA, G_MOVEWORD = 0xDB;
constexpr uint32_t G_RDPPIPESYNC = 0xE7, G_SETSCISSOR = 0xED;
// G_MTX's parameter byte, which F3DEX2 sends with G_MTX_PUSH flipped: set for a projection, clear for a modelview.
constexpr uint32_t G_MTX_PROJECTION = 0x04;
constexpr uint32_t G_MTX_MODELVIEW = 0x00;

// The sky's vertices, which func_80091110 loads with one G_VTX and gives new texture coordinates each frame: two
// buffers, used on alternate frames, as the display list is.
constexpr uint32_t SKY_VERTICES[2] = { 0x800C69C0, 0x800C6A60 };
// The sky's matrix group: any ID of the game's own that nothing else uses.
constexpr uint32_t SKY_GROUP_ID = 0x4D47534B;
// The cloud texture's period, in texels (its tile's mask), in the quarter texels gEXSetTexcoordWrapPoint takes.
constexpr uint32_t SKY_WRAP = 64 * 4;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The screen's width, in pixels.
constexpr int SCREEN_WIDTH = 320;
// The game's scissor for the 3D and the sky, in pixels: a rectangle past it is cut at the 4:3 frame's sides.
constexpr int SCISSOR_LEFT = 8;
constexpr int SCISSOR_RIGHT = 312;
constexpr int SCISSOR_TOP = 8;
constexpr int SCISSOR_BOTTOM = 232;
// How far back from the sky the game's last G_SETSCISSOR is looked for, in commands.
constexpr uint32_t SCISSOR_LOOKBACK = 4096;
// func_8008FF20 draws the sun's light only while the sun is less than this far past a side of the 4:3 screen (its
// -64 and 384), in pixels, whatever the light's half-width.
constexpr float SUN_REACH = 64.0f;
// Where func_8008FF20 keeps its sky's entry in the table at 0x800C6014 (its course, weather and time of day's): at 0xC4
// on its stack. The entry's word at 0x14 points to its sun, whose word at 0x10 is the light's half-width.
constexpr uint32_t SUN_ENTRY_ON_STACK = 0xC4;
constexpr uint32_t ENTRY_SUN = 0x14;
constexpr uint32_t SUN_SIZE = 0x10;

// Where func_8008FF20 keeps the sun's place on the screen, a float per view (two of them), x and then y, which it
// truncates to whole pixels for the light's rectangle.
constexpr uint32_t SUN_X = 0x800C5F24;
constexpr uint32_t SUN_Y = 0x800C5F2C;
constexpr int SUN_VIEWS = 2;
// The light's half-width, at most, in pixels (the game's size: 64 by day, 32 or 48 at other times, 96 at sunset).
constexpr int SUN_MAX_SIZE = 128;

// Above hud.cpp's buffers. Two lists a frame, and the renderer may read one a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80740000;
constexpr uint32_t BUFFER_SIZE = 0x400;
constexpr uint32_t BUFFER_COUNT = 64;
// The commands read from the game, at most.
constexpr uint32_t MAX_COMMANDS = 20000;
// Rectangles rewritten in one call, at most.
constexpr int MAX_RECTS = 4;
// The far hills' strips, drawn stretched to the picture, each in a buffer of its own: about twenty a call, two calls a
// frame, and enough for the frames the renderer may still read.
constexpr uint32_t STRIP_BASE = 0x807C0000;
constexpr uint32_t STRIP_SIZE = 0x40;
constexpr uint32_t STRIP_COUNT = 1024;
// A copy-mode rectangle's texture step, s5.10: four texels a pixel, as the RDP copies four pixels at a time.
constexpr int COPY_DSDX = 4 << 10;

uint8_t* g_rdram;
uint32_t g_start;
bool g_open = false;
// Whether the scissor the sky is drawn under is the 3D's whole one, so the light may be widened past it.
bool g_full = true;
uint32_t g_next_buffer = 0;
uint32_t g_next_strip = 0;
// The sun's light rectangles place_suns() found in this call, which widen() draws stretched whether or not they reach
// past the scissor.
uint32_t g_sun_rects[MAX_RECTS];
int g_sun_count = 0;

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

// Where the last G_SETTILESIZE of a tile starts it, and how the last G_SETTILE of it repeats its texture across s.
struct Tile {
    int uls = 0, ult = 0; // in texels
    int period = 0;       // texels after which s repeats, 0 where it is clamped or unmasked
};

// The rectangle at addr (its three commands) drawn past the scissor where it reaches past it, put back past the left
// side where the game cut it there, in a buffer that branches back after it. The sun's light (always) is drawn so
// even where it stays within the scissor, so it is drawn the same way in every frame. False if it stays within the
// scissor and isn't always drawn so, or didn't fit.
bool widen(uint32_t addr, const Tile& tile, float scale, bool always) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF); // quarter pixels
    int lrx = int((w0 >> 12) & 0xFFF), lry = int(w0 & 0xFFF);
    int s = int16_t(st >> 16), t = int16_t(st & 0xFFFF); // s10.5
    int dsdx = int16_t(d >> 16), dtdy = int16_t(d & 0xFFFF); // s5.10
    if (dsdx <= 0) {
        return false;
    }
    int start = tile.uls * 32, start_t = tile.ult * 32;
    int new_ulx = ulx, new_lrx = lrx, new_s = s;
    // s moves by dsdx / 128 a quarter pixel, and t by dtdy / 128. The cut is put back in whole pixels, as RT64 puts a
    // textured rectangle's edges on them, and s keeps what is left of it (place_sun's fraction of a pixel).
    if (ulx == 0 && s > start) {
        int cut = ((s - start) * 32 + dsdx - 1) / dsdx; // pixels, rounded up
        new_ulx = -cut * 4;
        new_s = s - int(std::lround(cut * dsdx / 32.0f));
        // Wholly past the left side, the game's right edge is cut to 0 too: the square is put back as wide as it is
        // tall, with what the game cut at the top. Wholly above the top as well, nothing of it is in the picture.
        if (lrx <= ulx) {
            if (dtdy <= 0 || lry <= uly) {
                return false;
            }
            new_lrx = new_ulx + (lry - uly) + (t - start_t) * 128 / dtdy;
        }
    }
    if (!always && new_ulx >= SCISSOR_LEFT * 4 && new_lrx <= SCISSOR_RIGHT * 4) {
        return false;
    }

    // Drawn stretched to the picture, as RT64 draws a rectangle across the whole screen: the game's own scissor (the
    // 3D's, 8 to 312) is then the 3D's in the picture too. So that the light keeps its shape, the rectangle is put
    // nearer the screen's middle and its texture stepped faster by as much as the stretch spreads them.
    int wide_dsdx = int(std::lround(dsdx * scale));
    if (wide_dsdx > INT16_MAX) {
        return false;
    }
    // RT64 puts a textured rectangle's left edge on the whole pixel at or right of it, and starts its texture there,
    // so the squeezed left edge goes on that pixel and the texture starts as much further on: the light stays where
    // it is to a fraction of a pixel (place_sun), not where the squeeze rounds it to, a stretched pixel's steps.
    float left = SCREEN_WIDTH * 2 + (new_ulx - SCREEN_WIDTH * 2) / scale; // quarter pixels
    int wide_ulx = int(std::ceil(left / 4.0f - 1e-4f)) * 4;
    new_s += int(std::lround((wide_ulx - left) * wide_dsdx / 128.0f));
    if (new_s < INT16_MIN || new_s > INT16_MAX) {
        return false;
    }
    // The right edge is the squeezed width on from that left edge, so the two move together: RT64 pairs a rectangle
    // whose size changed with the last frame's only while its edges moved a pixel apart at most, and the left edge's
    // whole pixel and the right one's quarter, each rounded its own way, came 1.25 pixels apart now and then, a frame
    // the light was then drawn where the game put it and not slid there, a twitch as the camera panned (issue 208).
    int wide_lrx = wide_ulx + int(std::lround((new_lrx - new_ulx) / scale));

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[3];
    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    gEXTextureRectangle(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, wide_ulx, uly, wide_lrx, lry,
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

// A strip of the far hills at addr (its three commands), drawn on to the wide picture's sides at the hills' own
// width rather than stretched to it: a copy-mode rectangle across the 3D's whole width whose texture repeats across s.
// It is drawn stretched to the picture (G_EX_ASPECT_STRETCH), so the 3D's scissor is the wide picture's, its texture
// stepped faster by as much and started as much earlier as the picture reaches past 4:3 on the left, so each texel
// stays one 4:3 pixel wide and where the game put it. It stays a G_TEXRECT, so border.cpp, which follows the branch to
// it, still runs it on through the game's border to the screen's sides and puts back the rows the game cut at its top.
// False if it isn't such a strip, or didn't fit.
bool stretch_strip(uint32_t addr, const Tile& tile, float scale) {
    uint32_t w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int ulx = int((w1 >> 12) & 0xFFF);         // quarter pixels
    int lrx = int((rd_w(addr) >> 12) & 0xFFF);
    int s = int16_t(st >> 16);                 // s10.5
    int dsdx = int16_t(d >> 16);               // s5.10
    // The game's strips run from the scissor's left side to a pixel short of its right one (RT64 takes them to it).
    if (dsdx != COPY_DSDX || tile.period <= 0 || ulx > SCISSOR_LEFT * 4 || lrx < (SCISSOR_RIGHT - 1) * 4) {
        return false;
    }
    int wide_dsdx = int(std::lround(dsdx * scale));
    if (wide_dsdx > INT16_MAX) {
        return false;
    }
    // The rectangle's left side is drawn (SCREEN_WIDTH / 2 - ulx) * (scale - 1) 4:3 pixels further left, a texel each
    // in copy mode, 32 of s. s is put within the texture's first period, which draws the same, so border.cpp may move
    // it back further as it runs the strip on to the screen's left side.
    const int period = tile.period * 32;
    if (period > INT16_MAX) {
        return false;
    }
    int new_s = s - int(std::lround((SCREEN_WIDTH / 2.0f - ulx / 4.0f) * (scale - 1.0f) * 32.0f));
    new_s %= period;
    if (new_s < 0) {
        new_s += period;
    }

    uint32_t buffer = STRIP_BASE + STRIP_SIZE * g_next_strip;
    g_next_strip = (g_next_strip + 1) % STRIP_COUNT;
    Writer out{ buffer, buffer + STRIP_SIZE };
    GfxCommand c[1];
    gEXEnable(c);
    out.put(c, 1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    out.put(rd_w(addr), w1);
    out.put(rd_w(addr + 8), (uint32_t(new_s & 0xFFFF) << 16) | (st & 0xFFFF));
    out.put(rd_w(addr + 16), (uint32_t(wide_dsdx & 0xFFFF) << 16) | (d & 0xFFFF));
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

float rd_f(uint32_t addr) {
    uint32_t w = rd_w(addr);
    float f;
    std::memcpy(&f, &w, sizeof f);
    return f;
}

// The sun's light where the sun is, to a thirty-second of a pixel: the rectangle at addr, if it is the light
// func_8008FF20 drew around the truncated place of one of its views' suns, its texture started earlier by what the
// truncation dropped, each way, so the light is drawn that much further right and down in the rectangle the game put
// on whole pixels. The rectangle stays as the game's, so RT64 still pairs it with the last frame's, and slides the
// texture's step between them as it does a scroll. True if it is the light.
bool place_sun(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF); // quarter pixels
    int lrx = int((w0 >> 12) & 0xFFF), lry = int(w0 & 0xFFF);
    int s = int16_t(st >> 16), t = int16_t(st & 0xFFFF);     // s10.5
    int dsdx = int16_t(d >> 16), dtdy = int16_t(d & 0xFFFF); // s5.10
    if (dsdx <= 0 || dtdy <= 0) {
        return false;
    }
    for (int view = 0; view < SUN_VIEWS; view++) {
        float x = rd_f(SUN_X + 4 * view), y = rd_f(SUN_Y + 4 * view);
        if (!(std::fabs(x) < 4096.0f) || !(std::fabs(y) < 4096.0f)) {
            continue;
        }
        // The game's own rectangle: from the sun's place less its size plus one to its place plus its size, each
        // side at 0 at least.
        int tx = int(x), ty = int(y), size = lry / 4 - ty;
        if (lry <= 0 || size <= 0 || size > SUN_MAX_SIZE || lrx != std::max(0, tx + size) * 4 ||
            ulx != std::max(0, tx - size + 1) * 4 || uly != std::max(0, ty - size + 1) * 4) {
            continue;
        }
        // A pixel is dsdx / 32 of s (s10.5, dsdx s5.10), and dtdy / 32 of t. The truncation is toward 0, so a sun past
        // the screen's left side (x below 0) is moved left by its fraction.
        int ds = int(std::lround((x - float(tx)) * float(dsdx) / 32.0f));
        int dt = int(std::lround((y - float(ty)) * float(dtdy) / 32.0f));
        int new_s = s - ds, new_t = t - dt;
        if (new_s >= INT16_MIN && new_s <= INT16_MAX && new_t >= INT16_MIN && new_t <= INT16_MAX) {
            wr_w(addr + 12, (uint32_t(new_s & 0xFFFF) << 16) | uint32_t(new_t & 0xFFFF));
        }
        return true;
    }
    return false;
}

void place_suns(uint32_t start, uint32_t end) {
    g_sun_count = 0;
    if (end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }
    for (uint32_t addr = start; addr + 24 <= end; addr += 8) {
        if ((rd_w(addr) >> 24) == G_TEXRECT && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
            (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
            if (place_sun(addr) && g_sun_count < MAX_RECTS) {
                g_sun_rects[g_sun_count++] = addr;
            }
            addr += 16;
        }
    }
}

bool is_sun(uint32_t addr) {
    return std::find(g_sun_rects, g_sun_rects + g_sun_count, addr) != g_sun_rects + g_sun_count;
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
    for (uint32_t addr = start; addr + 8 <= end; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        switch (w0 >> 24) {
            case G_SETTILE: {
                Tile& tile = tiles[(w1 >> 24) & 7];
                uint32_t cms = (w1 >> 8) & 3, masks = (w1 >> 4) & 0xF;
                // Clamped, or unmasked, s doesn't repeat; mirrored, it repeats every two mirrored copies.
                tile.period = ((cms & 2) != 0 || masks == 0) ? 0 : (1 << masks) * ((cms & 1) != 0 ? 2 : 1);
                break;
            }
            case G_SETTILESIZE: {
                Tile& tile = tiles[(w1 >> 24) & 7];
                tile.uls = int((w0 >> 12) & 0xFFF) / 4;
                tile.ult = int(w0 & 0xFFF) / 4;
                break;
            }
            case G_TEXRECT:
                if (addr + 24 <= end && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 && (rd_w(addr + 16) >> 24) == G_RDPHALF_2) {
                    const Tile& tile = tiles[(w1 >> 24) & 7];
                    if (!stretch_strip(addr, tile, scale) && done < MAX_RECTS &&
                        widen(addr, tile, scale, is_sun(addr))) {
                        done++;
                    }
                    addr += 16;
                }
                break;
        }
    }
}

// Whether the last G_SETSCISSOR before addr in the display list is the 3D's whole one (8, 8)-(312, 232), or covers
// it. True where none is found: the course's frame sets that one first.
bool full_scissor(uint32_t addr) {
    for (uint32_t n = 0; n < SCISSOR_LOOKBACK && addr >= 8 && (addr & 0x7FFFFF) >= 8; n++) {
        addr -= 8;
        uint32_t w0 = rd_w(addr);
        if ((w0 >> 24) == G_SETSCISSOR) {
            uint32_t w1 = rd_w(addr + 4);
            int ulx = int((w0 >> 12) & 0xFFF), uly = int(w0 & 0xFFF); // quarter pixels
            int lrx = int((w1 >> 12) & 0xFFF), lry = int(w1 & 0xFFF);
            return ulx <= SCISSOR_LEFT * 4 && uly <= SCISSOR_TOP * 4 && lrx >= SCISSOR_RIGHT * 4 &&
                   lry >= SCISSOR_BOTTOM * 4;
        }
    }
    return true;
}

// The sky's draw between start and end (its modelview matrix, the G_VTX of its vertices and its triangles) put in a
// matrix group whose texture coordinates RT64 interpolates. False if it isn't there or a buffer didn't fit.
bool tag_sky(uint32_t start, uint32_t end) {
    if (end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0) {
        return false;
    }
    for (uint32_t vtx = start; vtx + 8 <= end; vtx += 8) {
        const uint32_t vtx_address = rd_w(vtx + 4) & 0x7FFFFF;
        if ((rd_w(vtx) >> 24) != G_VTX ||
            (vtx_address != (SKY_VERTICES[0] & 0x7FFFFF) && vtx_address != (SKY_VERTICES[1] & 0x7FFFFF))) {
            continue;
        }
        // Its modelview matrix, a few commands before (a G_MOVEWORD between them).
        uint32_t mtx = 0;
        for (uint32_t a = vtx; a >= start + 8 && a + 32 > vtx;) {
            a -= 8;
            uint32_t op = rd_w(a) >> 24;
            if (op == G_MTX) {
                if ((rd_w(a) & G_MTX_PROJECTION) == 0) {
                    mtx = a;
                }
                break;
            }
            if (op != G_MOVEWORD) {
                break;
            }
        }
        // The command after its last triangle (a pipe sync comes between the G_VTX and them).
        uint32_t tris = vtx + 8;
        while (tris + 8 <= end && (rd_w(tris) >> 24) == G_RDPPIPESYNC) {
            tris += 8;
        }
        uint32_t after = tris;
        while (after + 8 <= end && ((rd_w(after) >> 24) == G_TRI1 || (rd_w(after) >> 24) == G_TRI2 ||
                                    (rd_w(after) >> 24) == G_QUAD)) {
            after += 8;
        }
        uint32_t after_op = rd_w(after) >> 24;
        if (mtx == 0 || after == tris || after + 8 > end || after_op == G_TEXRECT || after_op == G_RDPHALF_1 ||
            after_op == G_RDPHALF_2) {
            return false;
        }

        uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
        g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
        Writer open{ buffer, buffer + BUFFER_SIZE / 2 };
        Writer close{ buffer + BUFFER_SIZE / 2, buffer + BUFFER_SIZE };
        GfxCommand c[2];
        gEXEnable(c);
        open.put(c, 1);
        gEXSetTexcoordWrapPoint(c, SKY_WRAP, SKY_WRAP);
        open.put(c, 1);
        gEXMatrixGroupSimple(c, SKY_GROUP_ID, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                             G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_SKIP,
                             G_EX_COMPONENT_SKIP, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_COMPONENT_INTERPOLATE,
                             G_EX_COMPONENT_SKIP);
        open.put(c, 2);
        open.put(rd_w(mtx), rd_w(mtx + 4));
        open.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), (mtx + 8) & 0x1FFFFFFF);
        gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
        close.put(c, 1);
        close.put(rd_w(after), rd_w(after + 4));
        close.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), (after + 8) & 0x1FFFFFFF);
        if (!open.ok || !close.ok) {
            return false;
        }
        wr_w(mtx, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
        wr_w(mtx + 4, buffer & 0x1FFFFFFF);
        wr_w(after, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
        wr_w(after + 4, (buffer + BUFFER_SIZE / 2) & 0x1FFFFFFF);
        return true;
    }
    return false;
}

} // namespace

// The sun's x on the screen as func_8008FF20's bounds read it: moved toward the screen's middle by as much as the wide
// picture reaches further, so that the light is drawn while any of it is in the picture.
extern "C" float mg_sun_bound_x(uint8_t* rdram, uint32_t sp, float x) {
    float scale = mg_wide_scale();
    if (scale <= 1.0f || !g_full) {
        return x;
    }
    g_rdram = rdram;
    // The light reaches its own half-width past the sun: 96 pixels at sunset, past the game's 64.
    float reach = SUN_REACH;
    uint32_t entry = rd_w(sp + SUN_ENTRY_ON_STACK);
    if ((entry >> 23) == (0x80000000u >> 23)) {
        uint32_t sun = rd_w(entry + ENTRY_SUN);
        if ((sun >> 23) == (0x80000000u >> 23)) {
            int size = int(rd_w(sun + SUN_SIZE));
            if (size > 0 && size <= SUN_MAX_SIZE) {
                reach = float(size);
            }
        }
    }
    float half = SCREEN_WIDTH / 2.0f;
    return half + (x - half) * (half + SUN_REACH) / (half * scale + reach);
}

extern "C" void mg_backdrop_begin(uint8_t* rdram) {
    g_rdram = rdram;
    g_start = rd_w(MG_GFX_HEAD);
    g_open = true;
    g_full = full_scissor(g_start);
}

extern "C" void mg_backdrop_end(uint8_t* rdram) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    tag_sky(g_start, rd_w(MG_GFX_HEAD));
    place_suns(g_start, rd_w(MG_GFX_HEAD));
    if (g_full) {
        widen_all(g_start, rd_w(MG_GFX_HEAD));
    }
    g_full = true;
}
