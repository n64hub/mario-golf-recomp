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
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETCOMBINE = 0xFC;
constexpr uint32_t G_DL_PUSH = 0, G_DL_NOPUSH = 1;
// G_SETCOMBINE's for black, opaque: colour 0 and alpha 1 in both cycles.
constexpr uint32_t BLACK_W0 = 0xFCFFFFFF, BLACK_W1 = 0xFFFFFDFE;
// The picture's strips, at most.
constexpr uint32_t MAX_STRIPS = 38;
// func_8006C484's iris: its phase (0 closing in, then held; 1 held past its time; 2 opening; 3 done) and its counter,
// and the row of its circle's middle in the texture (across, its middle is the picture's, OPAQUE_CENTRE_X).
constexpr uint32_t IRIS_PHASE = 0x800C4014, IRIS_COUNTER = 0x800C4018;
constexpr float IRIS_Y = 113.0f;

constexpr float SCREEN_WIDTH = 320.0f, CENTRE_X = 160.0f;
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

// Free RDRAM after border.cpp's buffers: five of 12 KB, used in turn, a frame's sides each.
constexpr uint32_t SIDES_BASE = 0x807B1000;
constexpr uint32_t SIDES_SIZE = 0x3000;
constexpr uint32_t SIDES_COUNT = 5;

uint8_t* g_rdram;
uint32_t g_from = 0;
uint32_t g_next_buffer = 0;
uint32_t g_next_sides = 0;

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

// The strip at rect moved to a buffer, scaled there by screen.cpp, and back to the list after it.
void move_strip(uint32_t rect) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    for (uint32_t i = 0; i < 24; i += 4) {
        wr_w(out.pos + i, rd_w(rect + i));
    }
    out.pos += 24;
    refit_strip(buffer);
    out.put((G_DL << 24) | (G_DL_NOPUSH << 16), (rect + 24) & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    mg_screen_fill_range(g_rdram, buffer, buffer + 24);
    wr_w(rect, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(rect + 4, buffer & 0x1FFFFFFF);
}

// A black rectangle from x0 to x1 and y0 to y1 (quarter pixels).
void put_black(Writer& out, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    out.put((G_TEXRECT << 24) | (x1 << 12) | y1, (x0 << 12) | y0);
    out.put(G_RDPHALF_1 << 24, 0);
    out.put(G_RDPHALF_2 << 24, 0);
}

// The radius (in the iris's units: a word of two pixels across, a row down) inside which the picture is clear this
// frame, after func_8006C484 has drawn its rings: negative where none of it is, a lot where all of it is.
float clear_radius() {
    int32_t phase = int32_t(rd_w(IRIS_PHASE)), counter = int32_t(rd_w(IRIS_COUNTER));
    switch (phase) {
        // Closing: it has made the rings from 0x90 down to counter + 1 opaque (and the counter falls on below 0 as
        // the picture is held).
        case 0: return float(counter);
        // Opening: it has made the rings up to counter - 1 clear, once its counter has climbed back from below 0.
        case 2: return counter > 0 ? float(counter - 1) : -1.0f;
        case 1: return -1.0f;
        default: return 1.0e6f;
    }
}

// In a window wider than 4:3, the picture's sides from the window's edges to the 4:3 frame's (G_EX_ASPECT_STRETCH):
// black where the iris's circle, carried on past the picture's sides, has the picture opaque, row by row of the
// texture as the strips draw them. Then the game's combine back. False if nothing needs drawing.
bool put_sides(Writer& out, float wide, uint32_t combine_w0, uint32_t combine_w1) {
    constexpr float S = FILL * OPAQUE;
    float radius = clear_radius();
    if (radius >= 1.0e5f) {
        return false;
    }
    // The 4:3 frame's left side, stretched across the window: a quarter pixel into the frame, so no column of the
    // course shows between it and the picture.
    float left = std::ceil((CENTRE_X - CENTRE_X / wide) * 4.0f) / 4.0f;
    GfxCommand c[1];
    out.put(BLACK_W0, BLACK_W1);
    gEXSetRectAspect(c, G_EX_ASPECT_STRETCH);
    out.put(c, 1);
    // One rectangle a side for each of the texture's rows, at that row's place whatever the iris does, never one for a
    // run of rows: RT64 (RT64_INTERPOLATE_RECT_SCROLLS, CMakeLists.txt) pairs a rectangle with the frame before's at its
    // place, or else with the nearest of its size, and draws it between the two in the frames between the game's, so
    // runs that changed height each frame were drawn between other runs, in wedges and bars across the course.
    bool any = false;
    for (int row = int(OPAQUE_FROM); row < 224; row++) {
        uint32_t y0 = uint32_t(std::lround((row - OPAQUE_FROM) * S * 4.0f));
        uint32_t y1 = uint32_t(std::lround((row + 1 - OPAQUE_FROM) * S * 4.0f));
        float dy = row + 0.5f - IRIS_Y;
        float black_to = left;
        if (radius >= 0.0f && radius * radius > dy * dy) {
            // Where the clear words end across (each two pixels, measured to its middle), on the screen, then as a
            // stretched side's x.
            float half = 2.0f * std::sqrt(radius * radius - dy * dy) + 1.0f;
            float screen_x = CENTRE_X - half * S;
            black_to = std::fmin(left, CENTRE_X + (screen_x - CENTRE_X) / wide);
        }
        uint32_t x = uint32_t(std::lround(std::fmax(black_to, 0.0f) * 4.0f));
        if (x > 0 && y1 > y0) {
            put_black(out, 0, y0, x, y1);
            put_black(out, uint32_t(SCREEN_WIDTH * 4) - x, y0, uint32_t(SCREEN_WIDTH * 4), y1);
            any = true;
        }
    }
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    out.put(c, 1);
    out.put(combine_w0, combine_w1);
    out.put(G_ENDDL << 24, 0);
    return any && out.ok;
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
    uint32_t combine_w0 = 0, combine_w1 = 0, strips = 0;
    for (uint32_t addr = g_from; addr < end && strips < MAX_STRIPS; addr += 8) {
        uint32_t w0 = rd_w(addr);
        if ((w0 >> 24) == G_SETCOMBINE) {
            combine_w0 = w0;
            combine_w1 = rd_w(addr + 4);
        }
        else if (addr + 24 <= end && is_rect(addr)) {
            move_strip(addr);
            strips++;
            addr += 16;
        }
    }
    // The sides, called from the list after the strips.
    float wide = mg_wide_scale();
    if (strips > 0 && wide > 1.0f && combine_w0 != 0) {
        uint32_t buffer = SIDES_BASE + SIDES_SIZE * g_next_sides;
        Writer out{ buffer, buffer + SIDES_SIZE };
        if (put_sides(out, wide, combine_w0, combine_w1)) {
            g_next_sides = (g_next_sides + 1) % SIDES_COUNT;
            wr_w(end, (G_DL << 24) | (G_DL_PUSH << 16));
            wr_w(end + 4, buffer & 0x1FFFFFFF);
            wr_w(sp + HEAD_ON_STACK, end + 8);
        }
    }
    g_from = 0;
}
