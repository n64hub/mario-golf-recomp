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
// aims and swings and the ball flies, and the course isn't drawn again) or from its own copy (GAME_COPY: the pause's),
// whose columns past the border hold the course as it was drawn. The game loads each strip of either only inside the
// border, columns 8 to 311, so those loads are widened to the whole row with it, or the RDP's clamp repeated columns 8
// and 311 out to the picture's sides: bars over its outer edges as the aim turned, and behind the pause, the
// leaderboard a hole ends on and every menu over the course the game holds still (all drawn over its copy). The hills
// (func_80091110, backdrop.cpp's) are strips of a texture whose tops the game cuts at the border, the strip's texture
// starting as many rows further in (0x80091934-0x800919AC); a strip wholly above it is cut to nothing. A full-width
// rectangle at the border's top whose texture starts past its first row has those rows put back, up to 8, so the hills
// reach the picture's top where the course does; one wholly above the border has its bottom where it was. A strip that
// reaches past the screen's top, or lies wholly above it (its bottom wrapping to a large lry, which the RDP would draw
// down to the scissor's bottom), is drawn whole at its own place above the screen's top with RT64's texture rectangle,
// whose coordinates are signed, so each strip is the same rectangle from frame to frame as the camera tips: cut at the
// screen's top, RT64 drew the strips there off their place in the frames between, a band of sky across the picture's
// top or through the pyramids as the camera tipped down fast. Rows are put back only where they are the strip's own:
// the last strip, drawn down to the border's bottom whatever its rows, keeps its top at the border once all the hills
// are above it, where its texture starts past its rows and the RDP repeats them in a band the course covers (it
// streaked the picture's top 8 lines). One whose texture starts past its rows at the border itself, every line of it
// that band, runs on up to the screen's top with its texture as it is, so the top 8 lines are the band the line under
// them is, rather than the sky and the hills drawn above the picture (Mini-Golf's flyovers look steeply down on a hole,
// the whole backdrop above the frame: a strip of sky and cloud showed there, over the iris opening on one too). A
// copy of the frame that ends at the border's bottom runs on to the screen's. So
// does the hills' last strip where it already runs on past its 4 rows, its last row repeated down to the border as the
// game draws it (in the flyover's view from above, the course doesn't cover the screen, and nothing else draws its last
// 8 lines, which kept what an earlier frame left there); one no taller than its texture stays (the sky goes on below
// it, where the course doesn't). The strips the game cuts at the border's bottom, or leaves with nothing to draw below
// it, get their own rows back to the screen's bottom, so with the camera tipped up the hills reach it where the course
// does. One drawn in strips that each load only their own rows (the old frame Ring Shot's stage select holds while its
// slats close) has strips of the rows above its first and below its last drawn with them. One over the whole of the
// border's inside that copies no frame (the opening's fades) covers the whole screen. The pause's copy holds only the
// 224 lines inside the border (all 320 columns), so the pause keeps the black above and below it that the console
// draws, filled before its first strip (otherwise those lines kept whatever was drawn there before the pause), and its
// last strip, which the game runs on past the copy's end, stops at the border. Lakitu Mode's letterbox bars, rectangles
// as wide as the game's scissor, are hud.cpp's: they are drawn through its buffer, under the scissor this file makes,
// and reach the picture's edges by hud.cpp's hand.
//
// While the view holds still (a golfer aiming, or swinging, as the ball flies), the game draws the course again from
// its color image and its depth from its own copy of it: each frame clears the z-buffer and draws that copy, a whole
// 320x240 depth image at GAME_COPY, into it in strips (texture rectangles to a color image that is the z-buffer), and
// then draws what moves (the golfer, the water's ripples) against it. Its strips are the border's inside, so past the
// border the depth stayed cleared, as far as it goes, and the water there was drawn over the course held in the
// picture: at Toad Highlands hole 4's tee, flat strips of the pond along the window's left and bottom edges, over the
// tee's grass, that went as the view turned (issue 182). Those strips are widened as the picture's are, whole rows
// loaded, with the copy's rows above and below the border drawn after the first and the last, so the depth the game
// holds reaches the picture's edges, its own outermost columns past the 4:3 frame's sides.
//
// The sky (three triangles on one plane, from ten vertices, the first thing func_80091110 draws: backdrop.cpp's) ends a
// few lines below the screen's top at most of the opening's camera's heights, and on a hole wherever the camera tilts
// up (a ball's flight as the view follows it into the sky), its near edge (vertices 0, 3 and 4; at y 120 under the
// opening's camera, line 4) inside the border. Neither clears the screen, so those lines kept an older frame's picture:
// grass and trees over the opening's sky, and on a hole a band at the window's top that smeared as the view moved. So
// its G_VTX is pointed at a copy of the ten (in a buffer of this file's) whose near three are moved a tenth further
// along the plane, away from their far partners (1, 2 and between them), their texture coordinates and colours with
// them, so the sky's picture is the game's where it was and goes on above it, past the screen's top.
//
// nuGfxTaskStart sends each frame's display list, and us.toml hooks its start: mg_border_fill follows the list and the
// lists it calls, in RAM (the game's and this program's buffers), while the game is in the opening (overlay 7 loaded;
// the title goes back to it after a while, its attract demo) or on a hole (overlay 2) but not in the end credits
// (overlay 16), which draw the course in a box of their own. It rewrites what it finds in place, the game's lists that
// are its data too, so with Aspect Ratio Original, or elsewhere, it puts any scissor it made back as the game wrote it:
// Original keeps the console's border. The game sets the screen's scissor itself too, so the one made here carries a
// mark the RDP and RT64 never read. Only the scissors and rectangles drawn to the screen (a color image 320 wide, not
// the z-buffer) are changed, and the strips of the game's copy of the course's depth drawn to the z-buffer. The other
// files' buffers that the lists branch to hold RT64's extended commands, some longer than one command, which the walk
// steps over whole: read a command at a time, the words of the sun's light's extended rectangle (backdrop.cpp's) read
// as the end of the list wherever its texture started left of 0, so the far hills after it, which backdrop.cpp also
// draws from buffers of its own, were left at the 4:3 frame's sides.

#include <algorithm>
#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

bool mg_aspect_expand();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETSCISSOR = 0xED, G_SETTIMG = 0xFD, G_SETCIMG = 0xFF, G_SETZIMG = 0xFE, G_SPNOOP = 0x00;
constexpr uint32_t G_SETOTHERMODE_H = 0xE3, G_SETFILLCOLOR = 0xF7, G_FILLRECT = 0xF6, G_VTX = 0x01;
constexpr uint32_t G_SETTILE = 0xF5, G_LOADTILE = 0xF4, G_SETTILESIZE = 0xF2, G_RDPLOADSYNC = 0xE6, G_RDPPIPESYNC = 0xE7;
// RT64's extended commands (its rt64_extended_gbi.h), which this program's buffers hold among the game's: their
// opcode, and the length of each that is longer than one command. The rest of their words are no command of their own:
// gEXTextureRectangle's third holds s and t, whose top byte reads as any command where s is negative (the sun's light,
// widened by backdrop.cpp, as G_ENDDL), which ended the walk before the far hills.
constexpr uint32_t RT64_EXTENDED = 0x64;
uint32_t extended_length(uint32_t op) {
    switch (op) {
        case 0x02: // G_EX_TEXRECT_V1
        case 0x08: // G_EX_SETSCISSORALIGN_V1
            return 3;
        case 0x04: // G_EX_SETVIEWPORT_V1
        case 0x05: // G_EX_SETSCISSOR_V1
        case 0x06: // G_EX_SETRECTALIGN_V1
        case 0x07: // G_EX_SETVIEWPORTALIGN_V1
        case 0x0C: // G_EX_MATRIXGROUP_V1
        case 0x13: // G_EX_EDITGROUPBYADDRESS_V1
        case 0x14: // G_EX_VERTEX_V1
        case 0x30: // G_EX_MATRIX_FLOAT_V1
        case 0x31: // G_EX_SETVERTEXSEGMENT_V1
            return 2;
        default:
            return 1;
    }
}
// G_SETOTHERMODE_H's cycle type (G_MDSFT_CYCLETYPE) and its copy and fill values; black as a fill of 16-bit pixels.
constexpr uint32_t SET_CYCLE_TYPE = 0xE3000A01, G_CYC_COPY = 0x00200000, G_CYC_FILL = 0x00300000, BLACK = 0x00010001;
// Past billboard.cpp's buffers: 16 of 0x100 bytes.
constexpr uint32_t BUFFER_BASE = 0x807B0000, BUFFER_SIZE = 0x100, BUFFER_COUNT = 16;
// Below hud.cpp's buffers, the strips of the hills drawn above the screen's top (ext_rect): five commands each, a few a
// frame, two lists a frame, and the renderer may read one a frame or more after the game sends it.
constexpr uint32_t EXT_BASE = 0x806F0000, EXT_SIZE = 0x30, EXT_COUNT = 1024;

// The overlay table (the game's loader sets the byte at the head of an overlay's entry while it is loaded), a hole's
// overlay, the opening's and the end credits'.
constexpr uint32_t OVERLAYS = 0x800B5F58, OVERLAY_SIZE = 0x28;
constexpr uint32_t HOLE_OVERLAY = 2, OPENING_OVERLAY = 7, CREDITS_OVERLAY = 16;
// The sky's ten vertices (backdrop.cpp's SKY_VERTICES: two buffers, on alternate frames), 16 bytes each (Vtx_t: x, y,
// z, flag, s, t, red, green, blue, alpha), and how far its near ones are moved, as a share of the way to the far ones.
constexpr uint32_t SKY_VERTICES[2] = { 0x000C69C0, 0x000C6A60 }, SKY_COUNT = 10, VERTEX_BYTES = 16;
constexpr float SKY_REACH = 0.1f;
// Where a transition's copy of the frame before lies (screen.cpp's), and the game's RAM.
constexpr uint32_t GAME_COPY = 0x002EA000, GAME_COPY_END = GAME_COPY + 320 * 224 * 2, GAME_RAM = 0x00800000;
// The rows of the hills' texture each of their strips draws.
constexpr uint32_t STRIP_ROWS = 4;

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
// Whether the color image is the screen (320 wide, not the z-buffer) or the z-buffer itself (where the game draws its
// copy of the course's depth), the texture image, the z-buffer, the cycle type, and whether this frame's border has
// been filled black.
bool g_screen;
bool g_depth;
uint32_t g_texture;
uint32_t g_zimg;
uint32_t g_cycle;
bool g_blacked;
uint32_t g_next_buffer;
uint32_t g_next_ext;
// The last G_SETTIMG's first word (its format, size and width), the last G_LOADTILE, and each tile's last
// G_SETTILESIZE.
uint32_t g_timg0;
uint32_t g_load0, g_load1;
uint32_t g_size0[8], g_size1[8];
// Where each of those is in the list (and each tile's last G_SETTILE), so a strip's load can be widened.
uint32_t g_load_addr, g_size_addr[8], g_tile_addr[8];
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

// The rows outside the border beside a strip of a screen's color image that starts or ends at it (the old frame Ring
// Shot's stage select holds while its slats close, 6 rows a strip and 2 the last), or of the game's copy of the
// course's depth: the strip loads only its own rows,
// so drawn on past the border it showed rows its tile doesn't hold, black above it and its last row repeated below, a
// band of streaks. Here the strip stays as it is and strips of 4 rows each draw screen rows first to last (0 to 8, or
// 232 to 240), each loading its own rows of the same image as the game loads its strips (its tile's rows moved by as
// many, its t with them), then the game's own load and tile are put back for what follows. The commands go in one of
// the buffers black_border uses, the strip's place in the list calling them.
void outside_rows(uint32_t addr, uint32_t first, uint32_t last, uint32_t inside) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    uint32_t out = buffer;
    auto put = [&](uint32_t w0, uint32_t w1) {
        wr_w(out, w0);
        wr_w(out + 4, w1);
        out += 8;
    };
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t tile = (w1 >> 24) & 7;
    int32_t uly = int32_t(w1 & 0xFFF) / 4;
    for (uint32_t i = 0; i < 3; i++) {
        put(rd_w(addr + i * 8), rd_w(addr + i * 8 + 4));
    }
    for (uint32_t row = first; row < last; row += 4) {
        uint32_t rows = last - row < 4 ? last - row : 4;
        uint32_t ult = uint32_t(int32_t(g_load0 & 0xFFF) + (int32_t(row) - uly) * 4);
        uint32_t lrt = ult + (rows - 1) * 4;
        put(G_RDPLOADSYNC << 24, 0);
        put((g_load0 & ~0xFFFu) | ult, (g_load1 & ~0xFFFu) | lrt);
        put(G_RDPPIPESYNC << 24, 0);
        put((g_size0[tile] & ~0xFFFu) | ult, (g_size1[tile] & ~0xFFFu) | lrt);
        put((w0 & ~0xFFFu) | ((row + rows) * 4 - inside), (w1 & ~0xFFFu) | (row * 4));
        put(rd_w(addr + 8), (rd_w(addr + 12) & 0xFFFF0000) | (ult * 8));
        put(rd_w(addr + 16), rd_w(addr + 20));
    }
    put(G_RDPLOADSYNC << 24, 0);
    put(g_load0, g_load1);
    put(G_RDPPIPESYNC << 24, 0);
    put(g_size0[tile], g_size1[tile]);
    put(G_ENDDL << 24, 0);
    wr_w(addr, G_DL << 24);
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
    for (uint32_t i = 1; i < 3; i++) {
        wr_w(addr + i * 8, G_SPNOOP << 24);
        wr_w(addr + i * 8 + 4, 0);
    }
}

// The texture rectangle at addr drawn from uly to lry (quarter pixels, above the screen's top: negative) with its
// texture at t there, by RT64's texture rectangle, whose coordinates are signed: the rectangle's sides, tile, s and
// steps as they are now. Its commands go in a buffer of EXT_BASE's, after the command that enables RT64's (the
// rectangle may be in the game's own list, where none has), the rectangle's place in the list calling them.
void ext_rect(uint32_t addr, int32_t uly, int32_t lry, int32_t t) {
    uint32_t buffer = EXT_BASE + EXT_SIZE * g_next_ext;
    g_next_ext = (g_next_ext + 1) % EXT_COUNT;
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    int32_t ulx = int32_t((w1 >> 12) & 0xFFF), lrx = int32_t((w0 >> 12) & 0xFFF);
    int32_t s = int16_t(st >> 16), dsdx = int16_t(d >> 16), dtdy = int16_t(d & 0xFFFF);
    GfxCommand c[4];
    gEXEnable(&c[0]);
    gEXTextureRectangle(&c[1], G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, ulx, uly, lrx, lry, (w1 >> 24) & 7, s, t, dsdx,
                        dtdy);
    uint32_t out = buffer;
    for (const GfxCommand& cmd : c) {
        wr_w(out, cmd.values.word0);
        wr_w(out + 4, cmd.values.word1);
        out += 8;
    }
    wr_w(out, G_ENDDL << 24);
    wr_w(out + 4, 0);
    wr_w(addr, G_DL << 24);
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
    for (uint32_t i = 1; i < 3; i++) {
        wr_w(addr + i * 8, G_SPNOOP << 24);
        wr_w(addr + i * 8 + 4, 0);
    }
}

// Whether the strip at addr, of a screen's color image, reads it a row a line from its tile's first row, the image
// 16-bit and 4 of its rows a load that fits in TMEM, with as many rows of the image above its tile as the border's:
// what outside_rows draws the rows past the border with.
bool strip_of_rows(uint32_t addr) {
    uint32_t tile = (rd_w(addr + 4) >> 24) & 7;
    uint32_t t = rd_w(addr + 12) & 0xFFFF, dtdy = rd_w(addr + 20) & 0xFFFF;
    uint32_t ult = g_load0 & 0xFFF;
    return is_screen(g_texture) && dtdy == 0x400 && t == ult * 8 && (g_size0[tile] & 0xFFF) == ult &&
           ((g_timg0 >> 19) & 3) == 2 && ((g_timg0 & 0xFFF) + 1) * 2 * 4 <= 4096 && ult >= BORDER_TOP;
}

// The sky reaching past the screen's top: the G_VTX at addr pointed at a copy of its vertices with the near
// edge (0, 3 and 4) moved on along its plane, away from the far one (1, 2, and 4's between them). Left as it is unless
// the ten are laid out as the game draws them.
void sky(uint32_t addr) {
    uint32_t from = 0x80000000u | (rd_w(addr + 4) & 0x1FFFFFFF);
    struct Vertex {
        int32_t x, y, z, s, t, c[4];
        uint32_t flag;
    } v[SKY_COUNT];
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        uint32_t a = rd_w(from + i * VERTEX_BYTES), b = rd_w(from + i * VERTEX_BYTES + 4);
        uint32_t c = rd_w(from + i * VERTEX_BYTES + 8), d = rd_w(from + i * VERTEX_BYTES + 12);
        v[i] = { int16_t(a >> 16), int16_t(a), int16_t(b >> 16), int16_t(c >> 16), int16_t(c),
                 { int32_t(d >> 24), int32_t((d >> 16) & 0xFF), int32_t((d >> 8) & 0xFF), int32_t(d & 0xFF) },
                 b & 0xFFFF };
    }
    const Vertex &n0 = v[0], &n3 = v[3], &n4 = v[4], &f1 = v[1], &f2 = v[2];
    if (n0.z != n3.z || n0.z != n4.z || n0.y != n3.y || n0.y != n4.y || f1.z != f2.z || f1.y != f2.y ||
        n0.z <= f1.z || n0.x != f1.x || n3.x != f2.x || n4.x * 2 != n0.x + n3.x) {
        return;
    }
    Vertex mid = f1;
    mid.x = (f1.x + f2.x) / 2;
    mid.s = (f1.s + f2.s) / 2;
    mid.t = (f1.t + f2.t) / 2;
    for (int k = 0; k < 4; k++) {
        mid.c[k] = (f1.c[k] + f2.c[k]) / 2;
    }
    auto on = [](int32_t near, int32_t far, int32_t lo, int32_t hi) {
        float x = near + (near - far) * SKY_REACH;
        return x < lo ? lo : x > hi ? hi : int32_t(x + (x < 0 ? -0.5f : 0.5f));
    };
    const Vertex* fars[SKY_COUNT] = {};
    fars[0] = &f1;
    fars[3] = &f2;
    fars[4] = &mid;
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    for (uint32_t i = 0; i < SKY_COUNT; i++) {
        Vertex o = v[i];
        if (fars[i] != nullptr) {
            const Vertex& f = *fars[i];
            o.x = on(o.x, f.x, INT16_MIN, INT16_MAX);
            o.y = on(o.y, f.y, INT16_MIN, INT16_MAX);
            o.z = on(o.z, f.z, INT16_MIN, INT16_MAX);
            o.s = on(o.s, f.s, INT16_MIN, INT16_MAX);
            o.t = on(o.t, f.t, INT16_MIN, INT16_MAX);
            for (int k = 0; k < 4; k++) {
                o.c[k] = on(o.c[k], f.c[k], 0, 255);
            }
        }
        uint32_t out = buffer + i * VERTEX_BYTES;
        wr_w(out, (uint32_t(uint16_t(o.x)) << 16) | uint16_t(o.y));
        wr_w(out + 4, (uint32_t(uint16_t(o.z)) << 16) | o.flag);
        wr_w(out + 8, (uint32_t(uint16_t(o.s)) << 16) | uint16_t(o.t));
        wr_w(out + 12, (uint32_t(o.c[0]) << 24) | (uint32_t(o.c[1]) << 16) | (uint32_t(o.c[2]) << 8) | uint32_t(o.c[3]));
    }
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

// The strip of a screen's color image or of the game's copy of one (GAME_COPY, 320 wide) that the rectangle at addr
// draws, loaded whole across: the game loads only the
// border's inside, columns 8 to 311 (its G_LOADTILE and G_SETTILESIZE), and the RDP clamps a texel left of a tile's
// first column or right of its last to that column, so the strip run on to the screen's sides repeated columns 8 and
// 311 out to them, and RT64 drew those as bars over the picture's outer edges (8/320 of the window's width each side)
// in every frame the game drew again from its color image or its copy (the copy's strips 6 rows each, drawn in one
// cycle mode, dimmed behind the pause). Its load and tile size are made columns 0 to 319, and the
// line of both tiles (the load's and the strip's) as long, where the strip's rows still fit the 4 KB of TMEM. They
// are the game's commands, written anew each frame like the rectangles.
void whole_rows(uint32_t addr) {
    uint32_t tile = (rd_w(addr + 4) >> 24) & 7, load_tile = (g_load1 >> 24) & 7;
    constexpr uint32_t INSIDE_LEFT = 8 * 4, INSIDE_RIGHT = 311 * 4, LEFT = 0, RIGHT = 319 * 4;
    if (g_load_addr == 0 || g_size_addr[tile] == 0 || g_tile_addr[tile] == 0 || g_tile_addr[load_tile] == 0 ||
        ((g_timg0 >> 19) & 3) != 2 || (g_timg0 & 0xFFF) + 1 != 320 || ((g_load0 >> 12) & 0xFFF) != INSIDE_LEFT ||
        ((g_load1 >> 12) & 0xFFF) != INSIDE_RIGHT || ((g_size0[tile] >> 12) & 0xFFF) != INSIDE_LEFT ||
        ((g_size1[tile] >> 12) & 0xFFF) != INSIDE_RIGHT) {
        return;
    }
    // A 16-bit row of 320 texels is 80 of TMEM's 64-bit words.
    constexpr uint32_t LINE = 320 * 2 / 8;
    uint32_t rows = ((g_load1 & 0xFFF) - (g_load0 & 0xFFF)) / 4 + 1;
    const uint32_t tiles[2] = { tile, load_tile };
    for (uint32_t t : tiles) {
        uint32_t w0 = rd_w(g_tile_addr[t]);
        if ((w0 & 0x1FF) + rows * LINE > 4096 / 8) {
            return;
        }
    }
    for (uint32_t t : tiles) {
        uint32_t w0 = rd_w(g_tile_addr[t]);
        wr_w(g_tile_addr[t], (w0 & ~(0x1FFu << 9)) | (LINE << 9));
    }
    g_load0 = (g_load0 & ~(0xFFFu << 12)) | (LEFT << 12);
    g_load1 = (g_load1 & ~(0xFFFu << 12)) | (RIGHT << 12);
    g_size0[tile] = (g_size0[tile] & ~(0xFFFu << 12)) | (LEFT << 12);
    g_size1[tile] = (g_size1[tile] & ~(0xFFFu << 12)) | (RIGHT << 12);
    wr_w(g_load_addr, g_load0);
    wr_w(g_load_addr + 4, g_load1);
    wr_w(g_size_addr[tile], g_size0[tile]);
    wr_w(g_size_addr[tile] + 4, g_size1[tile]);
}

// The texture rectangle at addr (G_TEXRECT, then its texture coordinates, G_RDPHALF_1: s and t, and steps,
// G_RDPHALF_2: dsdx and dtdy), on a hole drawn at the frame's full height, if it is as wide as the 3D and drawn to the
// screen: its top at the border's put back as far as its texture starts past its first row, up to 8 rows of its tile, its bottom
// at the border's moved to the screen's where its texture is one of the screen's color images (in copy mode a
// rectangle's lower right is inside it, so there the border's last line is 231), and its sides at the screen's. That
// puts back the far hills' rows the game cut, and draws the frame before whole where the game copies it from its color
// image (as it does while a golfer swings and the ball flies, and the course isn't drawn again). The rectangles are
// written anew each frame, so this needs no undoing. A rectangle from the game's own copy of the course (GAME_COPY: the
// pause's) holds only the 224 lines inside the border, so the screen goes black before the first of it, as the console
// draws the border's lines; otherwise they kept whatever was drawn there before the pause, the course undimmed. One
// from card.cpp's copy, past the game's RAM, is card.cpp's. A strip of the game's copy drawn to the z-buffer (depth) is
// the course's depth, held: a whole image, so its first and last strips have the rows past the border drawn with them.
void rect(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    uint32_t ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF, lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF;
    bool game_copy = g_texture >= GAME_COPY && g_texture < GAME_COPY_END;
    bool depth = g_depth && game_copy;
    if (!g_fill || !(g_screen || depth) || g_texture >= GAME_RAM || ulx > WIDE_ULX || lrx < WIDE_LRX ||
        (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
        return;
    }
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
    // A strip of the hills whose rows lie more than the border's 8 lines above it reaches past the screen's top, where
    // the RDP can't put a rectangle (its coordinates are unsigned): the game draws it from the border's top, its texture
    // started as many rows further in, and its bottom above that top, which wraps to a large lry (12 bits) where it is
    // above the screen's top too. It is drawn instead whole, at its own place above the screen's top, with RT64's texture
    // rectangle, whose coordinates are signed (in a buffer of its own that enables RT64's commands: ext_rect), and the
    // screen's scissor cuts it. So each strip is the same rectangle frame to frame, moved as the camera moves, and RT64
    // slides them all alike in the frames between. Cut at the screen's top, one moving more than its 4 rows in a game
    // frame went unpaired or paired with another place: a band of the sky across the picture's top, or a line of it
    // through the pyramids, as the camera following a ball on Shy Guy Desert tipped down fast (issue 206). Where the
    // strip isn't one strip's rows (its bottom past its top's 4 rows, or above that top), the wrapped one draws nothing:
    // from the border's top on, the RDP would draw it down to the scissor's bottom, from texels past its rows.
    bool ext = false;
    int32_t ext_uly = 0, ext_lry = 0, ext_t = 0;
    if (!game_copy && copy && !is_screen(g_texture) && uly == BORDER_TOP) {
        int32_t t = int16_t(rd_w(addr + 12) & 0xFFFF);
        int32_t dtdy = int16_t(rd_w(addr + 20) & 0xFFFF);
        int32_t bottom = lry >= 0x800 ? int32_t(lry) - 0x1000 : int32_t(lry);
        if (t > 0 && dtdy > 0 && bottom < int32_t(BORDER_TOP)) {
            int32_t rows = t * 32 / dtdy;
            int32_t top = int32_t(uly) - rows * 4;
            if (top < 0 && bottom >= top && bottom < top + int32_t(STRIP_ROWS) * 4) {
                ext = true;
                ext_uly = top;
                ext_lry = bottom;
                ext_t = t - rows * dtdy / 32;
            }
        }
        if (!ext && lry >= 0x800) {
            wr_w(addr, w0 & ~0xFFFu);
            return;
        }
    }
    // A strip of a screen's color image has the rows above drawn by outside_rows.
    bool above = uly == BORDER_TOP && (depth || strip_of_rows(addr));
    if (!game_copy && !above && !ext && uly == BORDER_TOP && lry <= SCREEN_BOTTOM) {
        // t is 10.5 fixed point, dtdy 5.10: a row of the rectangle steps t by dtdy / 32.
        uint32_t half1 = rd_w(addr + 12);
        int32_t t = int16_t(half1 & 0xFFFF);
        int32_t dtdy = int16_t(rd_w(addr + 20) & 0xFFFF);
        if (t > 0 && dtdy > 0) {
            uint32_t rows = uint32_t(t * 32 / dtdy);
            if (rows > BORDER_ROWS) {
                rows = BORDER_ROWS;
            }
            int32_t top_t = t - int32_t(rows) * dtdy / 32;
            // The last of the lines put back that the rectangle draws (the line above the border's, or its own bottom
            // where that is higher: a strip of the hills wholly above the border), and the texture's row there. Past
            // its tile's last row (G_SETTILESIZE's lrt, 10.2) the RDP wraps round the tile's rows: those lines would
            // be a band of them, so none are put back. That is the hills' last strip, which the game always draws
            // down to the border's bottom, once the camera tips down and all the hills are above the picture: its
            // texture starts past its 4 rows, and the band it draws under the course was put into the top 8 lines,
            // streaks of the hills over the sky wherever the course didn't reach the picture's top (as the camera
            // following a ball at Shy Guy Desert's hole 1 tips down, issue 206).
            int32_t last_line = std::min(int32_t(uly) / 4, int32_t(lry + inside) / 4) - 1;
            int32_t last_t = top_t + (last_line - int32_t(uly / 4 - rows)) * dtdy / 32;
            uint32_t tile = (w1 >> 24) & 7;
            int32_t past_tile = int32_t((g_size1[tile] & 0xFFF) + 4) * 8;
            bool in_tile = g_size_addr[tile] == 0 || last_t < past_tile;
            if (in_tile) {
                wr_w(addr + 4, (w1 & ~0xFFFu) | (uly - rows * 4));
                wr_w(addr + 12, (half1 & 0xFFFF0000) | uint16_t(top_t));
            } else if (t >= past_tile && !is_screen(g_texture)) {
                // The strip's own first line is already past its tile's rows: every line it draws is the tile's last
                // row, repeated (the flat band the console shows from the border down, under the course). It runs on
                // up to the screen's top with its texture as it is, every line of it still past the tile, so the top
                // 8 lines carry on that band, as the line under them does, rather than show the sky and the strips
                // of the hills drawn above the picture there (Mini-Golf's flyovers, whose camera looks steeply down
                // on each hole, and the iris opening on one, issue 215).
                wr_w(addr + 4, w1 & ~0xFFFu);
            }
        }
    }
    w0 = rd_w(addr);
    // Only a copy of the frame has the lines below: a texture of the hills ends at their last row, which the RDP
    // would repeat down to the screen's bottom, a band of streaks over the sky where the flyover looks down. A strip
    // of the frame, which loads only its own rows, has the rows below drawn by outside_rows.
    bool below = false;
    if (is_screen(g_texture) && lry == BORDER_BOTTOM - inside) {
        if (strip_of_rows(addr)) {
            below = true;
        }
        else {
            w0 = (w0 & ~0xFFFu) | (SCREEN_BOTTOM - inside);
        }
    }
    else if (depth && lry == BORDER_BOTTOM - inside) {
        below = true;
    }
    // The hills' last strip, where it already runs on past its rows (the RDP repeating its last one down to the
    // border, as the game draws it), runs on to the screen's bottom the same way. The game draws it in copy mode to
    // 232 itself, a line past the border's last, so either bottom is the border's.
    else if (!game_copy && lry >= BORDER_BOTTOM - 4 && lry <= BORDER_BOTTOM && lry > uly + STRIP_ROWS * 4) {
        w0 = (w0 & ~0xFFFu) | (SCREEN_BOTTOM - inside);
    }
    // Any other strip of the hills the game cut at the border's bottom (0x80091958-0x80091970: its bottom the lesser of
    // its own and 232) gets its own rows back, up to the screen's bottom. The game goes on making strips down to a top
    // of 240 (0x80091578), so those below the border have their bottom above their top and draw nothing: with the
    // camera tipped up, the hills stood on a straight line 8 lines above the picture's bottom, the sky under them
    // where the course in front ran on down. A strip's own rows are its texture's, so nothing repeats.
    else if (!game_copy && lry == BORDER_BOTTOM && uly + STRIP_ROWS * 4 > lry + inside) {
        w0 = (w0 & ~0xFFFu) | std::min(uly + STRIP_ROWS * 4 - inside, SCREEN_BOTTOM - inside);
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
        if ((copy && is_screen(g_texture)) || game_copy) {
            whole_rows(addr);
        }
        wr_w(addr + 4, rd_w(addr + 4) & ~(0xFFFu << 12));
        wr_w(addr + 12, (uint32_t(uint16_t(s)) << 16) | (half1 & 0xFFFF));
    }
    if (lrx >= BORDER_RIGHT - 4 && lrx <= BORDER_RIGHT) {
        w0 = (w0 & ~(0xFFFu << 12)) | ((lrx + SCREEN_RIGHT - BORDER_RIGHT) << 12);
    }
    wr_w(addr, w0);
    if (ext) {
        ext_rect(addr, ext_uly, ext_lry, ext_t);
    }
    else if (above) {
        outside_rows(addr, 0, BORDER_TOP / 4, inside);
    }
    else if (below) {
        outside_rows(addr, BORDER_BOTTOM / 4, SCREEN_BOTTOM / 4, inside);
    }
    if (game_copy && !depth && !g_blacked) {
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
                g_depth = (w0 & 0xFFF) + 1 == 320 && (w1 & 0x1FFFFFFF) == g_zimg;
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
                g_timg0 = w0;
                break;
            case G_SETTILE:
                g_tile_addr[(w1 >> 24) & 7] = addr;
                break;
            case G_LOADTILE:
                g_load0 = w0;
                g_load1 = w1;
                g_load_addr = addr;
                break;
            case G_SETTILESIZE:
                g_size0[(w1 >> 24) & 7] = w0;
                g_size1[(w1 >> 24) & 7] = w1;
                g_size_addr[(w1 >> 24) & 7] = addr;
                break;
            case G_SETSCISSOR:
                scissor(addr);
                break;
            case G_VTX:
                if (g_fill && ((w0 >> 12) & 0xFF) == SKY_COUNT &&
                    ((w1 & 0x1FFFFFFF) == SKY_VERTICES[0] || (w1 & 0x1FFFFFFF) == SKY_VERTICES[1])) {
                    sky(addr);
                }
                break;
            case G_TEXRECT:
                rect(addr);
                addr += 16;
                g_commands += 2;
                break;
            case RT64_EXTENDED: {
                uint32_t more = extended_length(w0 & 0xFFFFFF) - 1;
                addr += 8 * more;
                g_commands += more;
                break;
            }
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
    g_depth = false;
    g_texture = 0;
    g_zimg = 0;
    g_cycle = 0;
    g_blacked = false;
    g_timg0 = 0;
    g_load0 = 0;
    g_load1 = 0;
    g_load_addr = 0;
    for (int i = 0; i < 8; i++) {
        g_size0[i] = 0;
        g_size1[i] = 0;
        g_size_addr[i] = 0;
        g_tile_addr[i] = 0;
    }
    uint32_t start = in_ram(list);
    if (start != 0) {
        walk(start, 0);
    }
}
