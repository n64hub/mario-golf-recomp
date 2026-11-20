// Ring Shot's stage-select slats over the whole picture (RT64's Aspect Ratio Expand): the wooden board that closes
// over a change of stage covers the course to the window's edges, where it covered only the 4:3 frame inside the game's
// own 8-pixel border, and the course went on showing at the sides and along the top and bottom while it was shut.
//
// Overlay 13's func_ovl13_802423A4 draws the slats: a 304x224 picture (8-bit, with its palette; the word at 0x80243FB0
// points to it) of planks with Mario in the middle, in 38 rows of 6 lines from line 8 down (the last 2 lines). For each
// row it loads that row's lines and draws two texture rectangles, one a half of the picture: the left from x 8 to the
// row's edge and the right from 320 less it to 312, each half's inner side at the edge, so they meet at 160 shut. The
// edge is how far the slats have closed (the word at 0x80243EE0, 0 to 0x1E0, which loader.cpp's header tells of), less
// 64 for each 7 rows down (at most 4 times), held between 8 and 160, so the rows close one band after another from the
// top. The rectangles are under the game's scissor of the whole screen (border.cpp makes it so while the course fills
// the border), but RT64 draws them in the 4:3 frame, as the game places them.
//
// So, while the course fills the picture (mg_border_fills), us.toml hooks the function's start and end, and each of the
// rectangles it adds is put back as pieces drawn stretched to the picture (G_EX_ASPECT_STRETCH, their places squeezed
// toward the middle and their texture stepped faster by as much, as ball.cpp's are). A half's edge goes from the
// picture's side (open) to the middle (shut) where the game's goes from 8 to 160, at the same pace, so the halves meet
// as the game's do, and Mario's picture comes to rest where the game puts it. Past the picture's outer side, a half goes
// on in plain planks, one rectangle as wide as the picture's side lies past the border when shut: the picture's 64
// columns (FILL) at its other side, which are plank in every row, running on from its side as if the picture repeated,
// then mirrored at FILL, and so on (a tile of their own, mirrored at that mask). The picture's planks run on across its
// sides, its first columns the light side of a seam where its last are the dark one, so a seam past the picture's side
// is dark then light as the one in the middle is: mirrored at the side, the left's was two light sides together and
// the right's two dark ones, a wide pale band and a wide dark one. Each rectangle moves
// whole with its half, at its size and with its texture, its part past the picture cut by the scissor, so that RT64
// slides it whole in the frames between the game's (half_pieces() says why). The top two rows are drawn
// again above line 8, and the last two below line 232, mirrored the same way at the line: each row's lines are in the
// texture memory only while it is drawn, so the first row draws lines 2-7 and the second 0-1, the last 232-233 and the
// one before it 234-239. Rows 0 and 1 close together, as do 36 and 37, so each pair's pieces are alike. The commands go
// to a buffer of this file's above the 4 MB the game uses, the rectangle's first command a branch to it and its end a
// branch back. With Aspect Ratio Original, or anywhere the course doesn't fill the border, the game's slats are drawn as
// it draws them.

#include <algorithm>
#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

float mg_wide_scale();
bool mg_border_fills(uint8_t* rdram);

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_DL_NOPUSH = 1;

// The game's slats, in quarter pixels: the rows' first line and height, the left half's left side and the right
// half's right side, and the picture's width (in texels) and the middle the halves meet at (in pixels).
constexpr int ROW_TOP = 8 * 4, ROW_HEIGHT = 6 * 4, ROWS = 38;
constexpr int LEFT_SIDE = 8 * 4, RIGHT_SIDE = 312 * 4;
constexpr int SCREEN_WIDTH = 320 * 4, SCREEN_HEIGHT = 240 * 4;
constexpr float HALF = 152.0f, MIDDLE = 160.0f, SIDE = 8.0f;
constexpr float PICTURE_WIDTH = 304.0f;
// The picture's outer columns on each side that are plank in every row, which fill the wide picture past it.
constexpr float FILL = 64.0f;

// Above the other files' buffers (backdrop.cpp's strips end at 0x807D0000), to RDRAM's end at 8 MB: 76 rectangles a
// frame, and the renderer may read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x807D0000;
constexpr uint32_t BUFFER_SIZE = 0x200;
constexpr uint32_t BUFFER_COUNT = 384;
// The commands read from the game, at most (the function adds about 500).
constexpr uint32_t MAX_COMMANDS = 2000;

uint8_t* g_rdram;
uint32_t g_head = 0;
uint32_t g_start = 0;
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

// The tiles the plain planks are drawn with, over the row's lines in texture memory as the game's tile 0 is: tile 1
// the picture's first FILL columns, which run on past its right side, tile 2 its last, which run on past its left,
// each mirrored every FILL columns (the RDP's mirror at a mask of FILL), so that one rectangle of either runs on in
// planks however wide it is.
constexpr int FILL_TILE_LEFT = 1, FILL_TILE_RIGHT = 2;
constexpr uint32_t FILL_MASK = 6; // FILL's 64 columns
constexpr uint32_t G_TX_MIRROR = 1;
constexpr uint32_t G_SETTILE = 0xF5, G_SETTILESIZE = 0xF2, G_RDPPIPESYNC = 0xE7;

// A piece of a half across: from x0 to x1 in the wide picture's pixels (the 4:3 frame's 0 to 320 within it), drawn
// with tile `tile`, its column s0 at x0 and one more each pixel to the right.
struct Piece {
    float x0, x1, s0;
    int tile;
};

// A band of lines down: from y0 to y1 in quarter pixels, its texture's line t0 at y0 (s10.5), stepping dtdy (s5.10).
struct Band {
    int y0, y1, t0, dtdy;
};

// The pieces of a half whose game edge is `edge` pixels from its side of the screen (8 open, 160 shut), in a picture
// `scale` times as wide as 4:3: its half of the picture, and past its outer side the planks, as wide as the picture's
// side lies past the 4:3 frame's border. Each keeps its size and its texture as the half moves, what lies past the
// picture cut by the scissor (the screen's), so that RT64 pairs it with itself in the game frame before and slides it
// whole in the frames between (RT64_INTERPOLATE_RECT_SCROLLS): a piece cut at the picture's side, that side held and
// its texture moving with the other, was drawn there with its texture smeared to a streak.
int half_pieces(bool left, float edge, float scale, Piece* out) {
    float outer = MIDDLE - MIDDLE * scale; // the picture's left side
    float lead = outer + (edge - SIDE) * (MIDDLE - outer) / (MIDDLE - SIDE); // the half's inner side, from the left
    float fill = SIDE - outer;
    int n = 0;
    if (lead <= outer) {
        return 0;
    }
    if (left) {
        float start = lead - HALF; // the picture's column 0
        out[n++] = { start, lead, 0.0f, 0 };
        if (start > outer) {
            out[n++] = { start - fill, start, FILL - fill, FILL_TILE_RIGHT }; // pixel start - 1 samples the last column
        }
    } else {
        float right_lead = 2.0f * MIDDLE - lead;
        float end = right_lead + HALF; // past the picture's last column
        out[n++] = { right_lead, end, HALF, 0 };
        if (end < 2.0f * MIDDLE - outer) {
            out[n++] = { end, end + fill, 0.0f, FILL_TILE_LEFT }; // pixel end samples column 0
        }
    }
    return n;
}

// The game's tile 0, as the last G_SETTILE and G_SETTILESIZE for it in the function's commands said.
struct Tile0 {
    uint32_t set0 = 0, size0 = 0, size1 = 0;
};

// The rectangle at addr (its three commands), row `row`, put back as its half's pieces in each of its bands. False if
// it didn't fit, or isn't drawn with tile 0 as the game's are.
bool rewrite(uint32_t addr, bool left, float edge, int row, float scale, const Tile0& tile0) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int uly = int(w1 & 0xFFF), lry = int(w0 & 0xFFF);
    int t = int16_t(st & 0xFFFF), dtdy = int16_t(d & 0xFFFF);
    if (((w1 >> 24) & 7) != 0 || (tile0.set0 >> 24) != G_SETTILE || (tile0.size0 >> 24) != G_SETTILESIZE) {
        return false;
    }

    Band bands[2];
    int band_count = 0;
    bands[band_count++] = { uly, lry, t, dtdy };
    // Above line 8 and below line 232, the edge rows' lines mirrored at the line.
    const int last = ROWS - 1;
    if (row == 0) {
        bands[band_count++] = { 2 * 4, ROW_TOP, 5 << 5, -(1 << 10) };
    } else if (row == 1) {
        bands[band_count++] = { 0, 2 * 4, 1 << 5, -(1 << 10) };
    } else if (row == last - 1) {
        bands[band_count++] = { 234 * 4, SCREEN_HEIGHT, 5 << 5, -(1 << 10) };
    } else if (row == last) {
        bands[band_count++] = { 232 * 4, 234 * 4, 1 << 5, -(1 << 10) };
    }

    Piece pieces[2];
    int piece_count = half_pieces(left, edge, scale, pieces);

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[3];
    gEXEnable(c);
    out.put(c, 1);
    // The plank tiles: tile 0's format, line and lines, at the picture's first or last FILL columns, mirrored.
    out.put(G_RDPPIPESYNC << 24, 0);
    for (int fill_tile : { FILL_TILE_LEFT, FILL_TILE_RIGHT }) {
        uint32_t tmem = fill_tile == FILL_TILE_LEFT ? 0 : uint32_t(PICTURE_WIDTH - FILL) / 8; // 8-bit texels, 8 a word
        out.put((tile0.set0 & ~0x1FFu) | (((tile0.set0 & 0x1FF) + tmem) & 0x1FF),
                (uint32_t(fill_tile) << 24) | (G_TX_MIRROR << 8) | (FILL_MASK << 4));
        out.put((G_SETTILESIZE << 24) | (tile0.size0 & 0xFFF),
                (uint32_t(fill_tile) << 24) | ((uint32_t(FILL) * 4 - 4) << 12) | (tile0.size1 & 0xFFF));
    }
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    // Stretched across the picture, a wide x is drawn at squeeze(x), and a texture step of one texel a pixel is scale.
    // Each piece is placed by its left side and keeps its width to the quarter pixel, and its texture, as it moves.
    auto squeeze = [scale](float x) { return (MIDDLE + (x - MIDDLE) / scale) * 4.0f; };
    int dsdx = int(std::lround(scale * 1024.0f));
    for (int p = 0; p < piece_count; p++) {
        const Piece& piece = pieces[p];
        int qx0 = int(std::lround(squeeze(piece.x0)));
        int qx1 = qx0 + int(std::lround((piece.x1 - piece.x0) * 4.0f / scale));
        if (qx1 <= 0 || qx0 >= SCREEN_WIDTH || qx0 < INT16_MIN || qx1 > INT16_MAX) {
            continue;
        }
        int s = int(std::lround(piece.s0 * 32.0f));
        for (int b = 0; b < band_count; b++) {
            const Band& band = bands[b];
            gEXTextureRectangle(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, qx0 & 0xFFFF, band.y0, qx1 & 0xFFFF, band.y1,
                                piece.tile, s & 0xFFFF, band.t0 & 0xFFFF, dsdx & 0xFFFF, band.dtdy & 0xFFFF);
            out.put(c, 3);
        }
    }
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

} // namespace

// head: the word the display list's head is kept in (the function's a0).
extern "C" void mg_slats_begin(uint8_t* rdram, uint32_t head) {
    g_rdram = rdram;
    g_head = head;
    g_start = rd_w(head);
}

extern "C" void mg_slats_end(uint8_t* rdram) {
    g_rdram = rdram;
    uint32_t start = g_start, end = g_head != 0 ? rd_w(g_head) : 0;
    g_head = 0;
    if (end <= start || (end - start) / 8 > MAX_COMMANDS || (start & 7) != 0 || !mg_border_fills(rdram)) {
        return;
    }
    float scale = std::max(1.0f, mg_wide_scale());
    Tile0 tile0;
    for (uint32_t addr = start; addr + 24 <= end; addr += 8) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        if ((w0 >> 24) == G_SETTILE && ((w1 >> 24) & 7) == 0) {
            tile0.set0 = w0;
            continue;
        }
        if ((w0 >> 24) == G_SETTILESIZE && ((w1 >> 24) & 7) == 0) {
            tile0.size0 = w0;
            tile0.size1 = w1;
            continue;
        }
        if ((w0 >> 24) != G_TEXRECT || (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
            continue;
        }
        int ulx = int((w1 >> 12) & 0xFFF), uly = int(w1 & 0xFFF);
        int lrx = int((w0 >> 12) & 0xFFF);
        int row = (uly - ROW_TOP) / ROW_HEIGHT;
        if (uly < ROW_TOP || (uly - ROW_TOP) % ROW_HEIGHT != 0 || row >= ROWS) {
            continue;
        }
        if (ulx == LEFT_SIDE) {
            rewrite(addr, true, lrx / 4.0f, row, scale, tile0);
        } else if (lrx == RIGHT_SIDE) {
            rewrite(addr, false, (SCREEN_WIDTH - ulx) / 4.0f, row, scale, tile0);
        }
        addr += 16;
    }
}
