// The course to the picture's edges (RT64's Aspect Ratio Expand): the 3D, the sky and the far hills go on through the
// game's own border on all four sides, rather than leaving 8 of the 240 lines black at the top and the bottom and 8 of
// the 320 columns at each side, which RT64 widens with the picture, in any window.
//
// The game draws every screen inside a border of its own, the scissor 8, 8 to 312, 232, the 304x224 a TV's edge left in
// sight. On a hole, and in the opening, the 3D's viewport is the whole 320x240, the sky three triangles in it and the
// far hills texture rectangles drawn before the course, so only that scissor kept them out of the border. RT64 widens
// the 3D past 4:3 by its scissor: the frame's widest one is the screen it maps to the window, so a 3D under 8 to 312
// ended 8/320 of the window's width short of each side. Here each such scissor is made the screen's, 0, 0 to 320, 240,
// and the course reaches the window's edges.
//
// RT64 also stretches across the window only a rectangle as wide as that widest scissor, so each texture rectangle as
// wide as the 3D drawn to the screen runs on to the screen's sides, its texture's start moved back with its left. Those
// are the far hills, and the frame before where the game draws it again from its color image (as it does while a golfer
// swings and the ball flies, and the course isn't drawn again) or from its own copy (GAME_COPY: the pause's), whose
// columns past the border hold the course as it was drawn. The hills (func_80091110, backdrop.cpp's) are strips of a
// texture whose tops the game cuts at the border, the strip's texture starting as many rows further in
// (0x80091934-0x800919AC); a strip wholly above it is cut to nothing. A full-width rectangle at the border's top whose
// texture starts past its first row has those rows put back, up to 8, so the hills reach the picture's top where the
// course does; one wholly above the border has its bottom where it was (above the screen's top, which wraps to a large
// lry, is left as it is). A copy of the frame that ends at the border's bottom runs on to the screen's. So does the
// hills' last strip where it already runs on past its 4 rows, its last row repeated down to the border as the game
// draws it (in the flyover's view from above, the course doesn't cover the screen, and nothing else draws its last 8
// lines, which kept what an earlier frame left there); one no taller than its texture stays (the sky goes on below it,
// where the course doesn't). One over the whole of
// the border's inside that copies no frame (the opening's fades) covers the whole screen. The pause's copy holds only
// the 224 lines inside the border (all 320 columns), so the pause keeps the black above and below it that the console
// draws, filled before its first strip (otherwise those lines kept whatever was drawn there before the pause), and its
// last strip, which the game runs on past the copy's end, stops at the border. Lakitu Mode's letterbox bars, rectangles
// as wide as the game's scissor, are hud.cpp's: they are drawn through its buffer, under the scissor this file makes,
// and reach the picture's edges by hud.cpp's hand.
//
// The sky (backdrop.cpp's) is three triangles of a plane, under a projection of its own, whose near edge the game moves
// each frame with the camera's pitch to a line or so below the screen's top; looking up (in the opening) it came 4
// lines down, inside the border, and those lines kept what an earlier frame left there. The sky's vertices are copied
// with the edge nearest the camera moved on along the plane, its position, texture coordinates and color carried on
// from the far edge's as the game's are (the plane's own mapping, so its picture inside the border is the same), until
// it lies 2 lines above the screen's top, and the sky is drawn from the copy.
//
// nuGfxTaskStart sends each frame's display list, and us.toml hooks its start: mg_border_fill follows the list and the
// lists it calls, in RAM (the game's and this program's buffers), while the game is in the opening (overlay 7 loaded;
// the title goes back to it after a while, its attract demo) or on a hole (overlay 2) but not in the end credits
// (overlay 16), which draw the course in a box of their own. It rewrites what it finds in place, the game's lists that
// are its data too, so with Aspect Ratio Original, or elsewhere, it puts any scissor it made back as the game wrote it:
// Original keeps the console's border. The game sets the screen's scissor itself too, so the one made here carries a
// mark the RDP and RT64 never read. Only the scissors and rectangles drawn to the screen (a color image 320 wide, not
// the z-buffer) are changed.

#include <algorithm>
#include <cmath>
#include <cstdint>

bool mg_aspect_expand();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETSCISSOR = 0xED, G_SETTIMG = 0xFD, G_SETCIMG = 0xFF, G_SETZIMG = 0xFE, G_SPNOOP = 0x00;
constexpr uint32_t G_SETOTHERMODE_H = 0xE3, G_SETFILLCOLOR = 0xF7, G_FILLRECT = 0xF6;
// G_SETOTHERMODE_H's cycle type (G_MDSFT_CYCLETYPE) and its copy and fill values; black as a fill of 16-bit pixels.
constexpr uint32_t SET_CYCLE_TYPE = 0xE3000A01, G_CYC_COPY = 0x00200000, G_CYC_FILL = 0x00300000, BLACK = 0x00010001;
// Past billboard.cpp's buffers: 16 of 0x100 bytes.
constexpr uint32_t BUFFER_BASE = 0x807B0000, BUFFER_SIZE = 0x100, BUFFER_COUNT = 16;

// The overlay table (the game's loader sets the byte at the head of an overlay's entry while it is loaded), a hole's
// overlay, the opening's and the end credits'.
constexpr uint32_t OVERLAYS = 0x800B5F58, OVERLAY_SIZE = 0x28;
constexpr uint32_t HOLE_OVERLAY = 2, OPENING_OVERLAY = 7, CREDITS_OVERLAY = 16;
// Where a transition's copy of the frame before lies (screen.cpp's), and the game's RAM.
constexpr uint32_t GAME_COPY = 0x002EA000, GAME_COPY_END = GAME_COPY + 320 * 224 * 2, GAME_RAM = 0x00800000;

// The border's scissor and the screen's, as G_SETSCISSOR's coordinates (quarter pixels). The game sets the screen's
// itself too (around its clears), so the one made here carries a mark in a bit of the second word neither the RDP nor
// RT64 reads, which tells it from the game's when it is put back.
constexpr uint32_t BORDER_UL = (8 * 4 << 12) | (8 * 4), BORDER_LR = (312 * 4 << 12) | (232 * 4);
constexpr uint32_t FULL_UL = 0, FULL_LR = (320 * 4 << 12) | (240 * 4);
constexpr uint32_t COORDS = 0xFFFFFF, MARK = 0x10000000;

// A full-width rectangle reaches from the border's left to its right (hud.cpp starts one at the screen's left).
constexpr uint32_t WIDE_ULX = 8 * 4, WIDE_LRX = 311 * 4;
constexpr uint32_t BORDER_TOP = 8 * 4, BORDER_BOTTOM = 232 * 4, SCREEN_BOTTOM = 240 * 4, BORDER_ROWS = 8;
constexpr uint32_t BORDER_RIGHT = 312 * 4, SCREEN_RIGHT = 320 * 4;
// The rows of the hills' texture each of their strips draws.
constexpr uint32_t STRIP_ROWS = 4;

// The sky's vertices (backdrop.cpp's): ten, in one of two buffers used on alternate frames. Their copies go past this
// file's buffers, in turn, as the renderer may read them a frame or more later.
constexpr uint32_t SKY_VERTICES[2] = { 0x800C69C0, 0x800C6A60 }, SKY_COUNT = 10, VERTEX_SIZE = 16;
constexpr uint32_t SKY_BASE = BUFFER_BASE + BUFFER_SIZE * BUFFER_COUNT, SKY_COPIES = 8;
constexpr uint32_t G_MTX = 0xDA, G_VTX = 0x01, G_TRI1 = 0x05, G_TRI2 = 0x06, G_QUAD = 0x07, G_RDPPIPESYNC = 0xE7;
// G_MTX's parameter byte (F3DEX2's): a projection, and a load rather than a multiply.
constexpr uint32_t MTX_PROJECTION = 0x04, MTX_LOAD = 0x02;
// Where the sky's near edge goes, as a y over w: 2 of the screen's 120 half-lines above its top. How far along the
// plane it may go, as a multiple of the near edge's distance from the far one.
constexpr float SKY_TOP = 1.0f + 2.0f / 120.0f, SKY_MAX_STRETCH = 8.0f;

constexpr int MAX_DEPTH = 10;
constexpr uint32_t MAX_COMMANDS = 400000;

uint8_t* g_rdram;
bool g_fill;
uint32_t g_commands;
// Whether the color image is the screen (320 wide, not the z-buffer), the texture image, the z-buffer, the cycle type,
// and whether this frame's border has been filled black.
bool g_screen;
uint32_t g_texture;
uint32_t g_zimg;
uint32_t g_cycle;
bool g_blacked;
uint32_t g_next_buffer;
// The last projection and modelview matrices loaded (0 for none, or a multiply), and the next copy of the sky.
uint32_t g_projection;
uint32_t g_modelview;
uint32_t g_next_sky;
// The screen's color images the game has drawn to (it keeps three, in turn), the latest first: a texture in one (each
// strip's texture image is its own first row) is a copy of a frame.
constexpr int SCREENS = 4;
constexpr uint32_t SCREEN_BYTES = 320 * 240 * 2;
uint32_t g_screens[SCREENS];

void saw_screen(uint32_t image) {
    for (int i = 0; i < SCREENS; i++) {
        if (g_screens[i] == image) {
            return;
        }
    }
    for (int i = SCREENS - 1; i > 0; i--) {
        g_screens[i] = g_screens[i - 1];
    }
    g_screens[0] = image;
}

bool is_screen(uint32_t image) {
    for (int i = 0; i < SCREENS; i++) {
        if (g_screens[i] != 0 && image >= g_screens[i] && image < g_screens[i] + SCREEN_BYTES) {
            return true;
        }
    }
    return false;
}

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

int16_t rd_h(uint32_t addr) {
    return *reinterpret_cast<int16_t*>(g_rdram + ((addr & 0x7FFFFF) ^ 2));
}

void wr_h(uint32_t addr, int16_t value) {
    *reinterpret_cast<int16_t*>(g_rdram + ((addr & 0x7FFFFF) ^ 2)) = value;
}

bool loaded(uint32_t overlay) {
    return g_rdram[((OVERLAYS + overlay * OVERLAY_SIZE) & 0x7FFFFF) ^ 3] != 0;
}

// The RAM address a display list command points at, or 0 for a segment or past RAM.
uint32_t in_ram(uint32_t addr) {
    uint32_t top = addr >> 24;
    if (top != 0x00 && top != 0x80 && top != 0xA0) {
        return 0;
    }
    addr &= 0x1FFFFFFF;
    return addr < GAME_RAM && (addr & 7) == 0 ? (0x80000000u | addr) : 0;
}

void scissor(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    if (g_fill && (w0 & COORDS) == BORDER_UL && (w1 & (COORDS | MARK)) == BORDER_LR) {
        wr_w(addr, (w0 & ~COORDS) | FULL_UL);
        wr_w(addr + 4, (w1 & ~COORDS) | FULL_LR | MARK);
    }
    else if (!g_fill && (w0 & COORDS) == FULL_UL && (w1 & (COORDS | MARK)) == (FULL_LR | MARK)) {
        wr_w(addr, (w0 & ~COORDS) | BORDER_UL);
        wr_w(addr + 4, (w1 & ~(COORDS | MARK)) | BORDER_LR);
    }
}

// The black the screen is filled with in a frame that draws the game's own copy of the course (the pause's picture):
// the commands before the copy's first strip, then the strip and a return. RDRAM above the 4 MB the game uses, past
// billboard.cpp's buffers, used in turn: the renderer may read a list a frame or more after the game sends it.
void black_border(uint32_t addr) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    uint32_t out = buffer;
    auto put = [&](uint32_t w0, uint32_t w1) {
        wr_w(out, w0);
        wr_w(out + 4, w1);
        out += 8;
    };
    put(SET_CYCLE_TYPE, G_CYC_FILL);
    put(G_SETFILLCOLOR << 24, BLACK);
    // The whole screen, which RT64 widens to the window's sides (a fill's corners are both inside it).
    put((G_FILLRECT << 24) | ((319 * 4) << 12) | (239 * 4), 0);
    put(SET_CYCLE_TYPE, g_cycle);
    for (uint32_t i = 0; i < 3; i++) {
        put(rd_w(addr + i * 8), rd_w(addr + i * 8 + 4));
    }
    put(G_ENDDL << 24, 0);
    wr_w(addr, G_DL << 24);
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
    for (uint32_t i = 1; i < 3; i++) {
        wr_w(addr + i * 8, G_SPNOOP << 24);
        wr_w(addr + i * 8 + 4, 0);
    }
}

// The texture rectangle at addr (G_TEXRECT, then its texture coordinates, G_RDPHALF_1: s and t, and steps,
// G_RDPHALF_2: dsdx and dtdy), on a hole drawn at the frame's full height, if it is as wide as the 3D and drawn to the
// screen: its top at the border's put back as far as its texture starts past its first row, up to 8 rows, its bottom
// at the border's moved to the screen's where its texture is one of the screen's color images (in copy mode a
// rectangle's lower right is inside it, so there the border's last line is 231), and its sides at the screen's. That
// puts back the far hills' rows the game cut, and draws the frame before whole where the game copies it from its color
// image (as it does while a golfer swings and the ball flies, and the course isn't drawn again). The rectangles are
// written anew each frame, so this needs no undoing. A rectangle from the game's own copy of the course (GAME_COPY: the
// pause's) holds only the 224 lines inside the border, so the screen goes black before the first of it, as the console
// draws the border's lines; otherwise they kept whatever was drawn there before the pause, the course undimmed. One
// from card.cpp's copy, past the game's RAM, is card.cpp's.
void rect(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF, lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
    if (!g_fill || !g_screen || g_texture >= GAME_RAM || ulx > WIDE_ULX || lrx < WIDE_LRX ||
        (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
        return;
    }
    bool game_copy = g_texture >= GAME_COPY && g_texture < GAME_COPY_END;
    bool copy = g_cycle == G_CYC_COPY;
    uint32_t inside = copy ? 4 : 0;
    // One over the whole of the border's inside, of no copy of a frame, is a picture over the screen (the opening's
    // fades, a small texture stretched): it covers the screen, its texture's start moved up by as many rows.
    if (!game_copy && !is_screen(g_texture) && uly == BORDER_TOP && lry == BORDER_BOTTOM - inside) {
        uint32_t half1 = rd_w(addr + 12);
        int32_t t = int16_t(half1 & 0xFFFF);
        int32_t dtdy = int16_t(rd_w(addr + 20) & 0xFFFF);
        t -= int32_t(uly) * dtdy / 128;
        wr_w(addr + 4, w1 & ~0xFFFu);
        wr_w(addr + 12, (half1 & 0xFFFF0000) | uint16_t(t));
        wr_w(addr, (w0 & ~0xFFFu) | (SCREEN_BOTTOM - inside));
        w0 = rd_w(addr);
        w1 = rd_w(addr + 4);
        uly = 0;
        lry = SCREEN_BOTTOM - inside;
    }
    // A strip wholly above the border has its bottom above the screen's top, which wraps past its bottom (lry is
    // unsigned): left as it is.
    if (!game_copy && uly == BORDER_TOP && lry <= SCREEN_BOTTOM) {
        // t is 10.5 fixed point, dtdy 5.10: a row of the rectangle steps t by dtdy / 32.
        uint32_t half1 = rd_w(addr + 12);
        int32_t t = int16_t(half1 & 0xFFFF);
        int32_t dtdy = int16_t(rd_w(addr + 20) & 0xFFFF);
        if (t > 0 && dtdy > 0) {
            uint32_t rows = uint32_t(t * 32 / dtdy);
            if (rows > BORDER_ROWS) {
                rows = BORDER_ROWS;
            }
            t -= int32_t(rows) * dtdy / 32;
            wr_w(addr + 4, (w1 & ~0xFFFu) | (uly - rows * 4));
            wr_w(addr + 12, (half1 & 0xFFFF0000) | uint16_t(t));
        }
    }
    w0 = rd_w(addr);
    // Only a copy of the frame has the lines below: a texture of the hills ends at their last row, which the RDP
    // would repeat down to the screen's bottom, a band of streaks over the sky where the flyover looks down.
    if (is_screen(g_texture) && lry == BORDER_BOTTOM - inside) {
        w0 = (w0 & ~0xFFFu) | (SCREEN_BOTTOM - inside);
    }
    // The hills' last strip, where it already runs on past its rows (the RDP repeating its last one down to the
    // border, as the game draws it), runs on to the screen's bottom the same way. The game draws it in copy mode to
    // 232 itself, a line past the border's last, so either bottom is the border's.
    else if (!game_copy && lry >= BORDER_BOTTOM - 4 && lry <= BORDER_BOTTOM && lry > uly + STRIP_ROWS * 4) {
        w0 = (w0 & ~0xFFFu) | (SCREEN_BOTTOM - inside);
    }
    // The pause's copy holds the 224 lines inside the border, and its last strip runs on past them, into what lies
    // after the copy: it stops at the border.
    if (game_copy && uly < BORDER_BOTTOM && lry > BORDER_BOTTOM - inside) {
        w0 = (w0 & ~0xFFFu) | (BORDER_BOTTOM - inside);
    }
    // Its sides at the screen's, its texture's start moved back with its left: s is 10.5 fixed point and dsdx 5.10, so
    // a quarter pixel steps s by dsdx / 128, or a quarter of that in copy mode, where dsdx is 4 for a texel a pixel.
    // RT64 stretches across the window only a rectangle as wide as the widest scissor the frame draws under, now the
    // screen's.
    if (ulx > 0) {
        uint32_t half1 = rd_w(addr + 12);
        int32_t s = int16_t(half1 >> 16);
        int32_t dsdx = int16_t(rd_w(addr + 20) >> 16);
        s -= int32_t(ulx) * dsdx / (copy ? 512 : 128);
        wr_w(addr + 4, rd_w(addr + 4) & ~(0xFFFu << 12));
        wr_w(addr + 12, (uint32_t(uint16_t(s)) << 16) | (half1 & 0xFFFF));
    }
    if (lrx >= BORDER_RIGHT - 4 && lrx <= BORDER_RIGHT) {
        w0 = (w0 & ~(0xFFFu << 12)) | ((lrx + SCREEN_RIGHT - BORDER_RIGHT) << 12);
    }
    wr_w(addr, w0);
    if (game_copy && !g_blacked) {
        g_blacked = true;
        black_border(addr);
    }
}

// An N64 matrix (s15.16, its integer halves then its fractions) at addr, and a row vector times it.
struct Matrix {
    float m[4][4];
};

Matrix read_matrix(uint32_t addr) {
    Matrix out;
    for (int i = 0; i < 16; i++) {
        int32_t whole = rd_h(addr + i * 2);
        uint32_t fraction = uint16_t(rd_h(addr + 32 + i * 2));
        out.m[i / 4][i % 4] = float(whole) + float(fraction) / 65536.0f;
    }
    return out;
}

void times(const float v[4], const Matrix& m, float out[4]) {
    for (int j = 0; j < 4; j++) {
        out[j] = v[0] * m.m[0][j] + v[1] * m.m[1][j] + v[2] * m.m[2][j] + v[3] * m.m[3][j];
    }
}

// A vertex of the sky: its position, texture coordinates and color, as numbers to carry on along the plane.
struct Vertex {
    float x, y, z, s, t, color[4];
};

Vertex read_vertex(uint32_t addr) {
    uint32_t rgba = rd_w(addr + 12);
    return { float(rd_h(addr)), float(rd_h(addr + 2)), float(rd_h(addr + 4)), float(rd_h(addr + 8)),
             float(rd_h(addr + 10)),
             { float(rgba >> 24), float((rgba >> 16) & 0xFF), float((rgba >> 8) & 0xFF), float(rgba & 0xFF) } };
}

Vertex mix(const Vertex& a, const Vertex& b, float k) {
    Vertex out;
    out.x = a.x + (b.x - a.x) * k;
    out.y = a.y + (b.y - a.y) * k;
    out.z = a.z + (b.z - a.z) * k;
    out.s = a.s + (b.s - a.s) * k;
    out.t = a.t + (b.t - a.t) * k;
    for (int c = 0; c < 4; c++) {
        out.color[c] = a.color[c] + (b.color[c] - a.color[c]) * k;
    }
    return out;
}

void clip(const Vertex& v, const Matrix& modelview, const Matrix& projection, float out[4]) {
    float p[4] = { v.x, v.y, v.z, 1.0f }, eye[4];
    times(p, modelview, eye);
    times(eye, projection, out);
}

// The sky's G_VTX at addr: where its triangles' edge nearest the camera is their top on the screen and lies below the
// screen's top, its vertices are copied with that edge carried on along the plane to SKY_TOP, and drawn from the copy.
void sky(uint32_t addr) {
    uint32_t w0 = rd_w(addr), source = in_ram(rd_w(addr + 4));
    uint32_t count = (w0 >> 12) & 0xFF, first = ((w0 >> 1) & 0x7F) - count;
    if (source == 0 || (source != SKY_VERTICES[0] && source != SKY_VERTICES[1]) || count != SKY_COUNT || first != 0 ||
        g_projection == 0 || g_modelview == 0) {
        return;
    }
    // The vertices its triangles use (a pipe sync may come between).
    bool used[SKY_COUNT] = {};
    for (uint32_t a = addr + 8;; a += 8) {
        uint32_t c0 = rd_w(a), c1 = rd_w(a + 4), op = c0 >> 24;
        if (op == G_RDPPIPESYNC) {
            continue;
        }
        if (op != G_TRI1 && op != G_TRI2 && op != G_QUAD) {
            break;
        }
        uint32_t words[2] = { c0, op == G_TRI1 ? 0 : c1 };
        for (uint32_t w : words) {
            for (int b = 0; b < 3; b++) {
                uint32_t index = ((w >> (16 - b * 8)) & 0xFF) / 2;
                if (index >= SKY_COUNT) {
                    return;
                }
                used[index] = true;
            }
        }
    }
    Vertex v[SKY_COUNT];
    float near_z = -1e9f, far_z = 1e9f;
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        v[i] = read_vertex(source + i * VERTEX_SIZE);
        if (used[i]) {
            near_z = std::max(near_z, v[i].z);
            far_z = std::min(far_z, v[i].z);
        }
    }
    // The far edge: two vertices of one height, apart on x. The near edge: the rest, of one height.
    int far[2], fars = 0;
    float near_y = 0;
    bool any_near = false;
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        if (!used[i]) {
            continue;
        }
        if (v[i].z == far_z) {
            if (fars == 2) {
                return;
            }
            far[fars++] = i;
        }
        else if (v[i].z == near_z) {
            if (any_near && v[i].y != near_y) {
                return;
            }
            near_y = v[i].y;
            any_near = true;
        }
        else {
            return;
        }
    }
    if (!any_near || fars != 2 || near_z <= far_z || v[far[0]].y != v[far[1]].y || v[far[0]].x == v[far[1]].x) {
        return;
    }
    Matrix modelview = read_matrix(g_modelview), projection = read_matrix(g_projection);
    // Each near vertex goes on from the far edge's point straight behind it: k times as far from it (clip space is
    // linear along the line, so k solves y = SKY_TOP * w there).
    float stretch = 1.0f;
    Vertex behind[SKY_COUNT];
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        if (!used[i] || v[i].z != near_z) {
            continue;
        }
        behind[i] = mix(v[far[0]], v[far[1]], (v[i].x - v[far[0]].x) / (v[far[1]].x - v[far[0]].x));
        float n[4], f[4];
        clip(v[i], modelview, projection, n);
        clip(behind[i], modelview, projection, f);
        if (n[3] <= 0 || f[3] <= 0 || n[1] / n[3] <= f[1] / f[3]) {
            return;
        }
        float denominator = (n[1] - f[1]) - SKY_TOP * (n[3] - f[3]);
        if (denominator <= 0) {
            return;
        }
        stretch = std::max(stretch, (SKY_TOP * f[3] - f[1]) / denominator);
    }
    if (stretch <= 1.0f) {
        return;
    }
    if (stretch > SKY_MAX_STRETCH) {
        return;
    }
    uint32_t copy = SKY_BASE + SKY_COUNT * VERTEX_SIZE * g_next_sky;
    Vertex moved[SKY_COUNT];
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        moved[i] = v[i];
        if (!used[i] || v[i].z != near_z) {
            continue;
        }
        moved[i] = mix(behind[i], v[i], stretch);
        float c[4];
        clip(moved[i], modelview, projection, c);
        // In front of the near plane, and every number within its field.
        if (c[3] <= 0 || c[2] < -c[3]) {
            return;
        }
        for (float value : { moved[i].y, moved[i].z, moved[i].s, moved[i].t }) {
            if (std::fabs(value) > 32767.0f) {
                return;
            }
        }
    }
    g_next_sky = (g_next_sky + 1) % SKY_COPIES;
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        uint32_t from = source + i * VERTEX_SIZE, to = copy + i * VERTEX_SIZE;
        for (uint32_t w = 0; w < VERTEX_SIZE; w += 4) {
            wr_w(to + w, rd_w(from + w));
        }
        if (!used[i] || v[i].z != near_z) {
            continue;
        }
        wr_h(to + 2, int16_t(std::lround(moved[i].y)));
        wr_h(to + 4, int16_t(std::lround(moved[i].z)));
        wr_h(to + 8, int16_t(std::lround(moved[i].s)));
        wr_h(to + 10, int16_t(std::lround(moved[i].t)));
        uint32_t rgba = 0;
        for (int c = 0; c < 4; c++) {
            rgba = (rgba << 8) | uint32_t(std::clamp(std::lround(moved[i].color[c]), 0L, 255L));
        }
        wr_w(to + 12, rgba);
    }
    wr_w(addr + 4, copy & 0x1FFFFFFF);
}

void walk(uint32_t list, int depth) {
    if (depth > MAX_DEPTH) {
        return;
    }
    for (uint32_t addr = list; g_commands < MAX_COMMANDS; addr += 8, g_commands++) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        switch (w0 >> 24) {
            case G_ENDDL:
                return;
            case G_DL: {
                uint32_t target = in_ram(w1);
                bool branch = ((w0 >> 16) & 0xFF) != 0;
                if (target != 0) {
                    walk(target, depth + (branch ? 0 : 1));
                }
                if (branch) {
                    return;
                }
                break;
            }
            case G_SETCIMG:
                g_screen = (w0 & 0xFFF) + 1 == 320 && (w1 & 0x1FFFFFFF) != g_zimg;
                if (g_screen) {
                    saw_screen(w1 & 0x1FFFFFFF);
                }
                break;
            case G_SETZIMG:
                g_zimg = w1 & 0x1FFFFFFF;
                break;
            case G_SETOTHERMODE_H:
                if (w0 == SET_CYCLE_TYPE) {
                    g_cycle = w1;
                }
                break;
            case G_SETTIMG:
                g_texture = w1 & 0x1FFFFFFF;
                break;
            case G_SETSCISSOR:
                scissor(addr);
                break;
            case G_MTX: {
                uint32_t matrix = (w0 & MTX_LOAD) != 0 ? in_ram(w1) : 0;
                if ((w0 & MTX_PROJECTION) != 0) {
                    g_projection = matrix;
                }
                else {
                    g_modelview = matrix;
                }
                break;
            }
            case G_VTX:
                if (g_fill) {
                    sky(addr);
                }
                break;
            case G_TEXRECT:
                rect(addr);
                addr += 16;
                g_commands += 2;
                break;
            default:
                break;
        }
    }
}

} // namespace

// Whether the course is drawn at the frame's full height now: on a hole, with Aspect Ratio Expand.
bool mg_border_fills(uint8_t* rdram) {
    g_rdram = rdram;
    return mg_aspect_expand() && (loaded(OPENING_OVERLAY) || (loaded(HOLE_OVERLAY) && !loaded(CREDITS_OVERLAY)));
}

extern "C" void mg_border_fill(uint8_t* rdram, uint32_t list) {
    g_fill = mg_border_fills(rdram);
    g_commands = 0;
    g_screen = true;
    g_texture = 0;
    g_zimg = 0;
    g_cycle = 0;
    g_blacked = false;
    g_projection = 0;
    g_modelview = 0;
    uint32_t start = in_ram(list);
    if (start != 0) {
        walk(start, 0);
    }
}
