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
// lry, is left as it is). A copy of the frame that ends at the border's bottom runs on to the screen's; the hills' last
// strip, whose texture ends there, stays (the sky goes on below it, where the course doesn't). One over the whole of
// the border's inside that copies no frame (the opening's fades) covers the whole screen. The pause's copy holds only
// the 224 lines inside the border (all 320 columns), so the pause keeps the black above and below it that the console
// draws, filled before its first strip (otherwise those lines kept whatever was drawn there before the pause), and its
// last strip, which the game runs on past the copy's end, stops at the border. Lakitu Mode's letterbox bars, rectangles
// as wide as the game's scissor, are hud.cpp's: they are drawn through its buffer, under the scissor this file makes,
// and reach the picture's edges by hud.cpp's hand.
//
// nuGfxTaskStart sends each frame's display list, and us.toml hooks its start: mg_border_fill follows the list and the
// lists it calls, in RAM (the game's and this program's buffers), while the game is in the opening (overlay 7 loaded;
// the title goes back to it after a while, its attract demo) or on a hole (overlay 2) but not in the end credits
// (overlay 16), which draw the course in a box of their own. It rewrites what it finds in place, the game's lists that
// are its data too, so with Aspect Ratio Original, or elsewhere, it puts any scissor it made back as the game wrote it:
// Original keeps the console's border. The game sets the screen's scissor itself too, so the one made here carries a
// mark the RDP and RT64 never read. Only the scissors and rectangles drawn to the screen (a color image 320 wide, not
// the z-buffer) are changed.

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
    uint32_t start = in_ram(list);
    if (start != 0) {
        walk(start, 0);
    }
}
