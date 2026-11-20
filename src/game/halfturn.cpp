// Tournament's HALF TURN picture at the picture's full height in any window, its shape kept, black past its sides, as
// screen.cpp draws the title and the menus and credits.cpp the credits' last picture (RT64's Aspect Ratio Expand).
//
// After the 9th hole's score card a Tournament round shows a 304x224 picture (asset 0x955, RGBA16, in a buffer the word
// at 0x800E1C00 points to, its pixels 8 bytes in) over the course: func_8006C918, which the frame calls after the
// course, the HUD and the score card, draws it as 38 texture rectangles of 6 rows each (the last 2) from x 8 to 312 and
// y 8 to 232, under the frame's scissor, in one cycle, point sampled, TEXEL0's colour at 1 - its alpha. It comes in
// and goes out as an iris through its own pixels: func_8006C484 clears (and later sets) their alpha bits along an
// ellipse about its middle, so it closes over the course from the picture's edges in and opens from its middle out.
// Drawn as the game draws it, each strip is as wide as the 3D, so border.cpp ran it on to the screen's sides (its
// texture's start moved back, the clamp repeating its first and last columns) and RT64 stretched it with the course,
// at the 4:3 frame's height: in a 16:9 window it stood right of and below its place, off the window's right edge, the
// course in bands at its left, top and bottom.
//
// So us.toml hooks the function where the strips begin (0x8006CB44, the list's head in its stack frame at sp + 0x28,
// the iris already drawn into the texture) and where it puts the head back (0x8006CD20). At the end each strip is
// moved into a buffer of this file's, whose first commands, the strip itself, go through screen.cpp
// (mg_screen_fill_range), which scales it to the picture's full height and keeps it at its shape
// (gEXSetRectAspect(G_EX_ASPECT_ADJUST)). The picture's first two rows and columns are never opaque (the iris never
// reaches them, so the course shows through there, under the border on a console), so the strip is first scaled about
// the picture's opaque part (refit_strip) for that to fill the height, or the course showed in a band across the
// window's top. In a window wider than 4:3 the buffer then draws, from the 4:3 frame's sides
// out to the window's (G_EX_ASPECT_STRETCH), black at the alpha of the strip's outer columns (2 and 303, the outermost
// the iris reaches), the strip's own texture read one column across, row by row as the strip is drawn. So the sides go
// black row by row as the iris closes over the picture's edges, and back to the course as it opens, as the console's
// picture covers its screen (the iris's edge is ragged there, as the game draws it, so a few rows of a side can stay
// black a frame longer than those round them). With Aspect Ratio Original nothing changes.

#include <cmath>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

extern "C" void mg_screen_fill_range(uint8_t* rdram, uint32_t list, uint32_t end);
extern "C" bool mg_screen_fills();
float mg_wide_scale();

namespace {

// Where func_8006C918 keeps the list's head while it draws: sp + 0x28.
constexpr uint32_t HEAD_ON_STACK = 0x28;
// F3DEX2's commands this reads and writes (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1, G_SETCOMBINE = 0xFC;
constexpr uint32_t G_DL_NOPUSH = 1;
// G_SETCOMBINE's colour adder (D) in each cycle, and its value for 0.
constexpr uint32_t COMBINE_D0_SHIFT = 15, COMBINE_D1_SHIFT = 6, COMBINE_ZERO = 7;
// The picture's strips, at most, and the columns of its texture whose alpha the sides take: the outermost the iris
// clears (func_8006C484 stops at 75 words, two pixels each, either side of word 76).
constexpr uint32_t MAX_STRIPS = 38;
constexpr int LEFT_COLUMN = 2, RIGHT_COLUMN = 303;

constexpr float SCREEN_WIDTH = 320.0f, SCREEN_HEIGHT = 240.0f, CENTRE_X = 160.0f, CENTRE_Y = 120.0f;
// The picture's first two rows and columns are never opaque (the iris reaches neither, so the course shows through
// them, under the border on a console): its opaque part, its rows 2 to 223 and columns 2 to 303, is scaled to fill the
// 224 lines inside the border by OPAQUE, its middle column (161 on the screen) to the screen's, before screen.cpp
// scales it on, or the course showed in a band across the window's top 8 of 720 lines.
constexpr float BORDER = 8.0f, OPAQUE_FROM = 2.0f, OPAQUE = 224.0f / (224.0f - OPAQUE_FROM);
constexpr float OPAQUE_CENTRE_X = BORDER + OPAQUE_FROM + (304.0f - OPAQUE_FROM) / 2.0f;
// Its scale: its 224 lines to the picture's 240, as screen.cpp scales a 2D screen.
constexpr float FILL = 240.0f / 224.0f;

// Between iris.cpp's buffers and callout.cpp's: 224 of 128 bytes, used in turn, six frames of strips.
constexpr uint32_t BUFFER_BASE = 0x80781000;
constexpr uint32_t BUFFER_SIZE = 0x80;
constexpr uint32_t BUFFER_COUNT = 224;

uint8_t* g_rdram;
uint32_t g_from = 0;
uint32_t g_next_buffer = 0;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + (addr & 0x7FFFFF));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + (addr & 0x7FFFFF)) = value;
}

bool is_rect(uint32_t addr) {
    return (rd_w(addr) >> 24) == G_TEXRECT && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
           (rd_w(addr + 16) >> 24) == G_RDPHALF_2;
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

// The first whole pixel at or past v, inside 0 to limit: screen.cpp's edge, so a side's rows are its strip's.
int edge(float v, float limit) {
    return int(std::ceil(std::fmin(std::fmax(v, 0.0f), limit) - 0.001f));
}

// A side of the strip at rect: from x0 to x1 (quarter pixels, stretched across the window), its rows those screen.cpp
// scales the strip's to, each reading the texel of column at the row the strip draws there. False if it has no rows.
bool put_side(Writer& out, uint32_t rect, uint32_t x0, uint32_t x1) {
    uint32_t w0 = rd_w(rect), w1 = rd_w(rect + 4), st = rd_w(rect + 12), d = rd_w(rect + 20);
    float uly = (w1 & 0xFFF) / 4.0f, lry = (w0 & 0xFFF) / 4.0f;
    float t = int16_t(st & 0xFFFF) / 32.0f, dtdy = int16_t(d & 0xFFFF) / 1024.0f;
    float b0 = CENTRE_Y + (uly - CENTRE_Y) * FILL, b1 = CENTRE_Y + (lry - CENTRE_Y) * FILL;
    int lo = edge(b0, SCREEN_HEIGHT), hi = edge(b1, SCREEN_HEIGHT);
    if (hi <= lo || x1 <= x0) {
        return false;
    }
    float step = dtdy / FILL;
    float t0 = t + (std::fmax(b0, 0.0f) - b0) * step;
    float t1 = t + (std::fmin(b1, SCREEN_HEIGHT) - b0) * step;
    int start = int(std::ceil(t0 * 32.0f - 0.001f));
    int t_step = int(std::trunc((t1 - start / 32.0f) / float(hi - lo) * 1024.0f));
    int column = x0 == 0 ? LEFT_COLUMN : RIGHT_COLUMN;
    out.put((G_TEXRECT << 24) | (x1 << 12) | uint32_t(hi * 4), (w1 & 0x07000000) | (x0 << 12) | uint32_t(lo * 4));
    out.put(G_RDPHALF_1 << 24, (uint32_t(column * 32) << 16) | (uint32_t(start) & 0xFFFF));
    out.put(G_RDPHALF_2 << 24, uint32_t(t_step) & 0xFFFF);
    return true;
}

// A coordinate of the game's picture (quarter pixels, a rectangle's) moved by OPAQUE about from, which goes to to.
uint32_t refit(uint32_t quarters, float to, float from) {
    float v = to + (quarters / 4.0f - from) * OPAQUE;
    return uint32_t(std::lround(std::fmax(v, 0.0f) * 4.0f)) & 0xFFF;
}

// The strip's copy at rect refitted (refit) about the opaque part, its texture's steps fitted to its new edges.
void refit_strip(uint32_t rect) {
    uint32_t w0 = rd_w(rect), w1 = rd_w(rect + 4), d = rd_w(rect + 20);
    uint32_t ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF, lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
    // Across about the middles of the two, so its sides lose as much; down from the border's top.
    uint32_t nulx = refit(ulx, CENTRE_X, OPAQUE_CENTRE_X), nlrx = refit(lrx, CENTRE_X, OPAQUE_CENTRE_X);
    uint32_t nuly = refit(uly, BORDER, BORDER + OPAQUE_FROM), nlry = refit(lry, BORDER, BORDER + OPAQUE_FROM);
    if (nlrx <= nulx || nlry <= nuly) {
        return;
    }
    // The texels it reads, across its new width and height.
    float s_step = int16_t(d >> 16) * float(lrx - ulx) / float(nlrx - nulx);
    float t_step = int16_t(d & 0xFFFF) * float(lry - uly) / float(nlry - nuly);
    wr_w(rect, (w0 & 0xFF000000) | (nlrx << 12) | nlry);
    wr_w(rect + 4, (w1 & 0xFF000000) | (nulx << 12) | nuly);
    wr_w(rect + 20, (uint32_t(int(std::trunc(s_step))) << 16) | (uint32_t(int(std::trunc(t_step))) & 0xFFFF));
}

// The strip at rect moved to a buffer: the strip, scaled by screen.cpp, then in a window wider than 4:3 the black
// sides at its outer columns' alpha (combine the game's with its colour 0), and back to the list after the strip.
void move_strip(uint32_t rect, uint32_t combine_w0, uint32_t combine_w1, float wide) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    for (uint32_t i = 0; i < 24; i += 4) {
        wr_w(out.pos + i, rd_w(rect + i));
    }
    out.pos += 24;
    refit_strip(buffer);
    if (wide > 1.0f && combine_w0 != 0) {
        // The 4:3 frame's sides, stretched across the window: a quarter pixel into the frame, so no column of the
        // course shows between them and the picture.
        uint32_t left = uint32_t(std::ceil((CENTRE_X - CENTRE_X / wide) * 4.0f));
        uint32_t right = uint32_t(std::floor((CENTRE_X + CENTRE_X / wide) * 4.0f));
        uint32_t black_w1 = combine_w1 | (COMBINE_ZERO << COMBINE_D0_SHIFT) | (COMBINE_ZERO << COMBINE_D1_SHIFT);
        GfxCommand c[1];
        out.put(combine_w0, black_w1);
        gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
        out.put(c, 1);
        put_side(out, buffer, 0, left);
        put_side(out, buffer, right, uint32_t(SCREEN_WIDTH * 4));
        gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
        out.put(c, 1);
        out.put(combine_w0, combine_w1);
    }
    out.put((G_DL << 24) | (G_DL_NOPUSH << 16), (rect + 24) & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    mg_screen_fill_range(g_rdram, buffer, buffer + 24);
    wr_w(rect, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(rect + 4, buffer & 0x1FFFFFFF);
}

} // namespace

extern "C" void mg_half_turn_begin(uint8_t* rdram, uint32_t sp) {
    g_rdram = rdram;
    g_from = mg_screen_fills() ? rd_w(sp + HEAD_ON_STACK) : 0;
}

extern "C" void mg_half_turn_end(uint8_t* rdram, uint32_t sp) {
    g_rdram = rdram;
    if (g_from == 0) {
        return;
    }
    uint32_t end = rd_w(sp + HEAD_ON_STACK);
    float wide = mg_wide_scale();
    uint32_t combine_w0 = 0, combine_w1 = 0, strips = 0;
    for (uint32_t addr = g_from; addr < end && strips < MAX_STRIPS; addr += 8) {
        uint32_t w0 = rd_w(addr);
        if ((w0 >> 24) == G_SETCOMBINE) {
            combine_w0 = w0;
            combine_w1 = rd_w(addr + 4);
        }
        else if (addr + 24 <= end && is_rect(addr)) {
            move_strip(addr, combine_w0, combine_w1, wide);
            strips++;
            addr += 16;
        }
    }
    g_from = 0;
}
