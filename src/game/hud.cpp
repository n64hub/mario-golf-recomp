// The course's HUD at the wide picture's sides (RT64's Aspect Ratio Expand): the hole's map and its par, rest and down
// at the left, the wind, the golfer's face and score at the right, the shot type and club at the bottom left and the lie
// at the bottom right, where the game draws them in its 4:3 frame, with the swing meter along the bottom kept in the
// middle. The sides are those of the picture RecompFrontend's HUD Placement gives the HUD (16:9 by default); at 4:3, or
// with Aspect Ratio Original, RT64 places everything as the game does.
//
// The game's code is asm in the decomp, so rather than a copy of the function that draws the HUD (func_80089094, 3,600
// instructions), the commands it adds to the frame's display list are put back with RT64's alignments between them.
// us.toml hooks its one call (in func_8002DC10, the frame's drawing) on both sides: mg_hud_begin notes where the list's
// head (the word at MG_GFX_HEAD) stands, and mg_hud_end reads what was added since. The HUD is texture rectangles and a
// few small models (the map, the ball on its lie, the wind's arrow) in an orthographic projection of the 320x240
// screen, each placed by a modelview matrix. So the commands are followed as the RSP would (the matrices, the viewport,
// the vertices, the display lists they call) to know where on the screen each draws, and they are split into pieces:
// a rectangle joins the piece before it when it lies within a few pixels of it, so a number stays with its label and
// the meter with its marks, and each model is a piece of its own, but for one drawn over another (the shot's banner and
// its letters as it spins). A piece across the middle of the screen, or near it,
// stays where the game draws it; one left of it is placed from the left side and one right of it from the right
// (gEXSetRectAlign for its rectangles, gEXSetViewportAlign and the viewport again for its models). A piece placed from
// a side draws under the game's scissors widened to the picture, a little inside the 3D's border (WIDE_MARGIN). A piece
// left in the middle keeps the game's own scissor: RT64 stretches a rectangle that spans the scissor's width across the
// picture, scissor and all, so Lakitu Mode's letterbox bars (two such rectangles) reach the 3D's border, where under the
// widened scissor they stopped short of it by the margin and left the course showing at their ends.
//
// The commands go to a buffer of this file's in RDRAM above the 4 MB the game uses (it sees 8 MB and never touches the
// upper 4), and the first command the game wrote is replaced by a branch to it, which branches back to where the game
// goes on. Eight buffers in turn, since the renderer may read a list a frame or more after the game sends it, and the
// game keeps two lists. The pause menu and the menus are drawn elsewhere and stay in the middle.
//
// The hole's sign at the top left (the course's picture with the hole's number, waving like a flag) is one of the
// models: a grid of 6x5 vertices (func_80085F98, called by func_8008658C) that the game moves in RAM each frame to wave
// it, at 0x800E21D0 and on, 0x1E0 bytes a player, under matrices that never change. RT64 draws between the game's frames
// by blending matrices, and with no group it leaves vertices as the game's next frame has them, so the sign stepped at
// the game's rate while the rest moved at the display's. So its vertex load is put in an RT64 matrix group of its own
// whose vertices RT64 interpolates (everything else as with no group), closed before the next load. The banner a shot
// comes to rest under, a quad with a model for each letter, spins and scales in and out over the course: its models
// are one piece, kept where the game draws it as it spins in from the left, so its letters stay on it in a wide
// picture, and one matrix group, so they keep their places on it in the frames between the game's, its letters drawn
// as models once more on the frame they turn to rectangles at its rest, so they turn with it to the end (anchor()
// says why).

#include <cstdint>
#include <cstring>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

bool mg_border_fills(uint8_t* rdram);

namespace {

// F3DEX2's commands and flags this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_VTX = 0x01, G_DL = 0xDE, G_ENDDL = 0xDF, G_MTX = 0xDA, G_POPMTX = 0xD8, G_MOVEMEM = 0xDC;
constexpr uint32_t G_TEXRECT = 0xE4, G_TEXRECTFLIP = 0xE5, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1, G_FILLRECT = 0xF6, G_SETSCISSOR = 0xED;
constexpr uint32_t G_MV_VIEWPORT = 8, G_DL_NOPUSH = 1, G_SC_NON_INTERLACE = 0;
constexpr uint32_t G_MTX_PUSH = 1, G_MTX_LOAD = 2, G_MTX_PROJECTION = 4, G_MTX_MODELVIEW = 0;
// gSPViewport's first word.
constexpr uint32_t G_VIEWPORT_W0 = 0xDC080008;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The full-screen viewport the HUD draws in (the game's own, set before the HUD), re-sent when its alignment changes.
constexpr uint32_t MG_HUD_VIEWPORT = 0x800B77A0;
// The hole's sign: its vertex load's first word (30 vertices), and the vertices it loads, one set a player.
constexpr uint32_t SIGN_VTX_W0 = 0x0101E03C;
constexpr uint32_t SIGN_VERTICES = 0x800E21D0, SIGN_VERTICES_SIZE = 4 * 0x1E0;
// Its matrix group's ID, away from the other groups' (src/game/backdrop.cpp's sky is 0x4D47534B).
constexpr uint32_t SIGN_GROUP_ID = 0x4D475347;
// The shot's banner: its quad's vertex load (4 vertices), whatever it says, and its matrix group's ID.
constexpr uint32_t BANNER_VTX_W0 = 0x01004008, BANNER_VERTICES = 0x800C5A18;
constexpr uint32_t BANNER_GROUP_ID = 0x4D474252;
// The HUD's scissor, set before it.
constexpr int MG_HUD_SCISSOR_ULX = 8, MG_HUD_SCISSOR_ULY = 8, MG_HUD_SCISSOR_LRX = 312, MG_HUD_SCISSOR_LRY = 232;

// How far from the HUD's sides its scissors stay. RT64 widens the 3D's scissor (8 to 312) with the picture, so its
// black border is 8/320 of the picture's width: 10.7 at 16:9, 14 at 21:9. Nothing clears what lies past it, so a HUD
// drawn there (as its pieces slide in) would stay on the screen; a margin of 14 keeps the HUD inside the 3D with the
// HUD placed at 16:9 in any window, or at the window's sides up to 21:9.
constexpr int WIDE_MARGIN = 14;

constexpr int SCREEN_WIDTH = 320, SCREEN_HEIGHT = 240;
constexpr int SCREEN_MIDDLE = SCREEN_WIDTH / 2;
// A piece whose middle is this close to the screen's stays in the middle.
constexpr int MIDDLE_ZONE = 40;
// What lies within this many pixels of a piece joins it.
constexpr int PIECE_GAP = 12;

constexpr uint32_t BUFFER_BASE = 0x80700000;
constexpr uint32_t BUFFER_SIZE = 0x8000;
constexpr uint32_t BUFFER_COUNT = 8;
// The commands read from the game, at most: more than this and the HUD is left as the game drew it.
constexpr uint32_t MAX_COMMANDS = 3000;

uint8_t* g_rdram;
// Whether this frame's hole is drawn at the frame's full height (border.cpp).
bool g_full;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

int16_t rd_h(uint32_t addr) {
    uint32_t w = rd_w(addr);
    return int16_t((addr & 2) ? (w & 0xFFFF) : (w >> 16));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

void wr_h(uint32_t addr, uint16_t value) {
    uint32_t w = rd_w(addr);
    wr_w(addr, (addr & 2) ? ((w & 0xFFFF0000u) | value) : ((w & 0xFFFFu) | (uint32_t(value) << 16)));
}

// A KSEG0 or segment 0 address in RDRAM (the game's lists use KSEG0 addresses, segment 0); 0 for any other segment.
uint32_t resolve(uint32_t seg_addr) {
    if ((seg_addr & 0x0F000000) != 0) {
        return 0;
    }
    return 0x80000000u | (seg_addr & 0x7FFFFF);
}

struct Mat {
    float m[4][4];
};

Mat identity() {
    Mat r{};
    for (int i = 0; i < 4; i++) {
        r.m[i][i] = 1.0f;
    }
    return r;
}

// a * b, row vectors (v * a * b), as the RSP multiplies.
Mat mul(const Mat& a, const Mat& b) {
    Mat r{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) {
                s += a.m[i][k] * b.m[k][j];
            }
            r.m[i][j] = s;
        }
    }
    return r;
}

// Adds to an N64 fixed-point Mtx's element (row i, column j).
void add_mtx(uint32_t addr, int i, int j, float value) {
    int idx = i * 4 + j;
    int32_t old = int32_t((uint32_t(uint16_t(rd_h(addr + idx * 2))) << 16) | uint16_t(rd_h(addr + 32 + idx * 2)));
    int32_t fixed = old + int32_t(value * 65536.0f);
    wr_h(addr + idx * 2, uint16_t(uint32_t(fixed) >> 16));
    wr_h(addr + 32 + idx * 2, uint16_t(uint32_t(fixed) & 0xFFFF));
}

// An N64 fixed-point Mtx: sixteen integer halves, then sixteen fraction halves.
Mat read_mtx(uint32_t addr) {
    Mat r{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            int idx = i * 4 + j;
            uint32_t whole = uint16_t(rd_h(addr + idx * 2));
            uint32_t frac = uint16_t(rd_h(addr + 32 + idx * 2));
            r.m[i][j] = float(int32_t((whole << 16) | frac)) / 65536.0f;
        }
    }
    return r;
}

struct Box {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    bool empty() const { return x1 < x0; }
    void add(float x, float y) {
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    void add(const Box& b) {
        if (!b.empty()) {
            add(b.x0, b.y0);
            add(b.x1, b.y1);
        }
    }
    bool contains(float x, float y) const { return !empty() && x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    bool inside(const Box& b) const { return !empty() && x0 >= b.x0 && x1 <= b.x1 && y0 >= b.y0 && y1 <= b.y1; }
    // Whether either box's middle lies inside the other.
    bool overlaps_middle(const Box& b) const {
        return contains((b.x0 + b.x1) * 0.5f, (b.y0 + b.y1) * 0.5f) || b.contains((x0 + x1) * 0.5f, (y0 + y1) * 0.5f);
    }
    bool near(const Box& b) const {
        return !empty() && !b.empty() && b.x0 <= x1 + PIECE_GAP && x0 <= b.x1 + PIECE_GAP && b.y0 <= y1 + PIECE_GAP &&
               y0 <= b.y1 + PIECE_GAP;
    }
};

// The RSP's state as far as where things land on the screen.
struct Rsp {
    Mat proj = identity();
    Mat mv[10];
    int depth = 0;
    float vp_scale_x = 160.0f, vp_scale_y = 120.0f, vp_trans_x = 160.0f, vp_trans_y = 120.0f;

    Rsp() { mv[0] = identity(); }

    void viewport(uint32_t vp) {
        vp_scale_x = rd_h(vp + 0) / 4.0f;
        vp_scale_y = rd_h(vp + 2) / 4.0f;
        vp_trans_x = rd_h(vp + 8) / 4.0f;
        vp_trans_y = rd_h(vp + 10) / 4.0f;
    }

    void matrix(uint32_t w0, uint32_t w1) {
        uint32_t addr = resolve(w1);
        if (addr == 0) {
            return;
        }
        uint32_t params = (w0 & 0xFF) ^ G_MTX_PUSH;
        Mat m = read_mtx(addr);
        if (params & G_MTX_PROJECTION) {
            proj = (params & G_MTX_LOAD) ? m : mul(m, proj);
            return;
        }
        if ((params & G_MTX_PUSH) && depth < 9) {
            mv[depth + 1] = mv[depth];
            depth++;
        }
        mv[depth] = (params & G_MTX_LOAD) ? m : mul(m, mv[depth]);
    }

    void pop(uint32_t w1) {
        int n = int(w1 / 64);
        depth = depth > n ? depth - n : 0;
    }

    // Where a vertex load's vertices land.
    Box vertices(uint32_t w0, uint32_t w1) {
        Box box;
        uint32_t addr = resolve(w1);
        uint32_t n = (w0 >> 12) & 0xFF;
        if (addr == 0) {
            return box;
        }
        Mat mvp = mul(mv[depth], proj);
        for (uint32_t i = 0; i < n && i < 64; i++) {
            float v[4] = { float(rd_h(addr + i * 16)), float(rd_h(addr + i * 16 + 2)), float(rd_h(addr + i * 16 + 4)), 1.0f };
            float c[4];
            for (int j = 0; j < 4; j++) {
                c[j] = v[0] * mvp.m[0][j] + v[1] * mvp.m[1][j] + v[2] * mvp.m[2][j] + v[3] * mvp.m[3][j];
            }
            if (c[3] <= 0.0f) {
                continue;
            }
            box.add(vp_trans_x + c[0] / c[3] * vp_scale_x, vp_trans_y - c[1] / c[3] * vp_scale_y);
        }
        return box;
    }

    // Where a called display list draws, followed with this state (and changing it, as the RSP would).
    Box call(uint32_t addr, int level) {
        Box box;
        for (int i = 0; i < 2000 && addr != 0 && level < 8; i++, addr += 8) {
            uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
            switch (w0 >> 24) {
                case G_VTX: box.add(vertices(w0, w1)); break;
                case G_MTX: matrix(w0, w1); break;
                case G_POPMTX: pop(w1); break;
                case G_MOVEMEM:
                    if ((w0 & 0xFF) == G_MV_VIEWPORT && resolve(w1) != 0) {
                        viewport(resolve(w1));
                    }
                    break;
                case G_DL:
                    box.add(call(resolve(w1), level + 1));
                    if (((w0 >> 16) & 0xFF) == G_DL_NOPUSH) {
                        return box;
                    }
                    break;
                case G_ENDDL: return box;
            }
        }
        return box;
    }
};

// A command as read, and what is put before it.
struct Cmd {
    uint32_t w0, w1;
    int piece; // the piece it starts drawing for, or -1
};

struct Piece {
    Box box;
    bool models = false;
    uint32_t origin = G_EX_ORIGIN_NONE;
    // Its first and last drawing commands.
    uint32_t first = 0, last = 0;
};

// Commands from open to close (not included) drawn in a matrix group: the shot's banner.
struct Span {
    uint32_t open, close;
};
constexpr int MAX_SPANS = 8;

Cmd g_cmds[MAX_COMMANDS];
Piece g_pieces[MAX_COMMANDS];

// Whether a piece is the shot's banner: models that start with its quad.
bool is_banner(const Piece& piece) {
    const Cmd& quad = g_cmds[piece.first];
    return piece.models && quad.w0 == BANNER_VTX_W0 && quad.w1 == BANNER_VERTICES;
}

bool is_triangle(const Cmd& cmd) {
    uint32_t op = cmd.w0 >> 24;
    return op == 0x05 || op == 0x06 || op == 0x07;
}

// The banner's letters as the last frame drew them as models: its commands from after its quad's triangles to its
// group's end, and the matrices they load (index into g_letter_mtx, or -1 for a command that loads none).
constexpr uint32_t MAX_LETTER_CMDS = 1024, MAX_LETTER_MATRICES = 32, MTX_SIZE = 64;
uint32_t g_letter_cmds[MAX_LETTER_CMDS][2];
int g_letter_mtx_of[MAX_LETTER_CMDS];
uint32_t g_letter_count = 0;
uint32_t g_letter_mtx[MAX_LETTER_MATRICES][MTX_SIZE / 4];
uint32_t g_letter_mtx_count = 0;
// Whether the last frame's HUD kept them.
bool g_letters_kept = false;

// Keeps the banner's letters, from the command after its quad's triangles to close. False if they don't fit.
bool keep_letters(uint32_t from, uint32_t close) {
    g_letter_count = 0;
    g_letter_mtx_count = 0;
    while (from < close && is_triangle(g_cmds[from])) {
        from++;
    }
    for (uint32_t i = from; i < close; i++) {
        const Cmd& cmd = g_cmds[i];
        if (g_letter_count == MAX_LETTER_CMDS) {
            return false;
        }
        int mtx = -1;
        if ((cmd.w0 >> 24) == G_MTX) {
            uint32_t addr = resolve(cmd.w1);
            if (addr == 0 || (cmd.w0 & G_MTX_PROJECTION) != 0 || g_letter_mtx_count == MAX_LETTER_MATRICES) {
                return false;
            }
            for (uint32_t w = 0; w < MTX_SIZE / 4; w++) {
                g_letter_mtx[g_letter_mtx_count][w] = rd_w(addr + w * 4);
            }
            mtx = int(g_letter_mtx_count++);
        }
        g_letter_cmds[g_letter_count][0] = cmd.w0;
        g_letter_cmds[g_letter_count][1] = cmd.w1;
        g_letter_mtx_of[g_letter_count] = mtx;
        g_letter_count++;
    }
    return g_letter_mtx_count > 0;
}

uint32_t g_start;
bool g_open = false;
uint32_t g_next_buffer = 0;

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

void rect_align(Writer& out, uint32_t origin) {
    GfxCommand c[2];
    int offset = origin == G_EX_ORIGIN_RIGHT ? -SCREEN_WIDTH * 4 : 0;
    gEXSetRectAlign(c, origin, origin, offset, 0, offset, 0);
    out.put(c, 2);
}

void viewport_align(Writer& out, uint32_t origin, uint32_t viewport_w1) {
    GfxCommand c[2];
    int offset = origin == G_EX_ORIGIN_RIGHT ? -SCREEN_WIDTH * 4 : 0;
    gEXSetViewportAlign(c, origin, offset, 0);
    out.put(c, 2);
    // RT64 places a viewport by the alignment in effect when it is set.
    out.put(G_VIEWPORT_W0, viewport_w1);
}

void scissor(Writer& out, uint32_t mode, int ulx, int uly, int lrx, int lry) {
    GfxCommand c[2];
    uint32_t lorigin = ulx < SCREEN_MIDDLE ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
    uint32_t rorigin = lrx <= SCREEN_MIDDLE ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
    if (lorigin == G_EX_ORIGIN_LEFT && ulx < WIDE_MARGIN) {
        ulx = WIDE_MARGIN;
    }
    if (rorigin == G_EX_ORIGIN_RIGHT && lrx > SCREEN_WIDTH - WIDE_MARGIN) {
        lrx = SCREEN_WIDTH - WIDE_MARGIN;
    }
    gEXSetScissor(c, mode, lorigin, rorigin, lorigin == G_EX_ORIGIN_RIGHT ? ulx - SCREEN_WIDTH : ulx, uly,
                  rorigin == G_EX_ORIGIN_RIGHT ? lrx - SCREEN_WIDTH : lrx, lry);
    out.put(c, 2);
}

// The game's scissor, unaligned: RT64 places it as the game's, and stretches it with a rectangle that spans it.
void game_scissor(Writer& out, uint32_t mode, int ulx, int uly, int lrx, int lry) {
    GfxCommand c[2];
    gEXSetScissor(c, mode, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, ulx, uly, lrx, lry);
    out.put(c, 2);
}

// RT64 places the left edge of a rectangle it stretches across the picture (one that spans the scissor's width) a pixel
// or two right of the stretched scissor's, which is the 3D's border: Lakitu Mode's letterbox bars, texture rectangles
// from the scissor's left to its right (8 to 312), left a column of the course showing at their left ends. Such a
// rectangle starts at the screen's left instead, its texture moved with it, and the scissor cuts it at the 3D's border.
// Its texture coordinates are the next command's (G_RDPHALF_1: s and t), its steps the one after (G_RDPHALF_2).
// On a hole drawn to the screen's edges (border.cpp), such a rectangle at the border's top or bottom (a bar) runs on to
// the screen's top or bottom and its right side, its texture moved with its top, or the course showed past the bars:
// true then, and it is drawn under the screen's whole scissor, the rest of the HUD under the game's own, which keeps
// what the game hides behind the border (the swing meter below it before it rises) hidden.
bool spanning_rect_from_left(Cmd& rect, Cmd& half1, const Cmd& half2, int sc_ulx, int sc_uly, int sc_lrx, int sc_lry) {
    uint32_t ulx = (rect.w1 >> 12) & 0xFFF, lrx = (rect.w0 >> 12) & 0xFFF;
    uint32_t uly = rect.w1 & 0xFFF, lry = rect.w0 & 0xFFF;
    if (ulx == 0 || ulx != uint32_t(sc_ulx) * 4 || lrx != uint32_t(sc_lrx) * 4 || (half1.w0 >> 24) != G_RDPHALF_1 ||
        (half2.w0 >> 24) != G_RDPHALF_2) {
        return false;
    }
    // s and t are 10.5 fixed point, dsdx and dtdy 5.10, ulx and uly in quarter pixels.
    int32_t s = int16_t(half1.w1 >> 16), t = int16_t(half1.w1 & 0xFFFF);
    int32_t dsdx = int16_t(half2.w1 >> 16), dtdy = int16_t(half2.w1 & 0xFFFF);
    s -= int32_t(ulx) * dsdx / 128;
    rect.w1 &= ~(0xFFFu << 12);
    bool tall = false;
    if (g_full && sc_uly == MG_HUD_SCISSOR_ULY && sc_lry == MG_HUD_SCISSOR_LRY) {
        if (uly == uint32_t(sc_uly) * 4) {
            t -= int32_t(uly) * dtdy / 128;
            rect.w1 &= ~0xFFFu;
            tall = true;
        }
        if (lry == uint32_t(sc_lry) * 4) {
            rect.w0 = (rect.w0 & ~0xFFFu) | uint32_t(SCREEN_HEIGHT * 4);
            tall = true;
        }
        // The course goes on to the screen's sides too, and RT64 stretches across the window only a rectangle as wide
        // as the widest scissor the frame draws under, now the screen's: the bar runs on to its right side.
        if (tall) {
            rect.w0 = (rect.w0 & ~(0xFFFu << 12)) | (uint32_t(SCREEN_WIDTH * 4) << 12);
        }
    }
    half1.w1 = (uint32_t(uint16_t(s)) << 16) | uint16_t(t);
    return tall;
}

void anchor(uint32_t start, uint32_t end) {
    bool letters_kept = g_letters_kept;
    g_letters_kept = false;
    uint32_t count = (end - start) / 8;
    if (end <= start || count > MAX_COMMANDS || (start & 7) != 0) {
        return;
    }

    // Read the commands, and where each draws.
    Rsp rsp;
    rsp.viewport(MG_HUD_VIEWPORT);
    int pieces = 0;
    for (uint32_t i = 0; i < count; i++) {
        Cmd& cmd = g_cmds[i];
        cmd.w0 = rd_w(start + i * 8);
        cmd.w1 = rd_w(start + i * 8 + 4);
        cmd.piece = -1;
        Box box;
        bool model = false;
        switch (cmd.w0 >> 24) {
            case G_TEXRECT:
            case G_TEXRECTFLIP:
            case G_FILLRECT:
                // w0 holds the lower right corner and w1 the upper left, in quarter pixels.
                box.add(float((cmd.w1 >> 12) & 0xFFF) / 4.0f, float(cmd.w1 & 0xFFF) / 4.0f);
                box.add(float((cmd.w0 >> 12) & 0xFFF) / 4.0f, float(cmd.w0 & 0xFFF) / 4.0f);
                break;
            case G_VTX:
                box = rsp.vertices(cmd.w0, cmd.w1);
                model = true;
                break;
            case G_DL:
                box = rsp.call(resolve(cmd.w1), 0);
                model = true;
                break;
            case G_MTX: rsp.matrix(cmd.w0, cmd.w1); break;
            case G_POPMTX: rsp.pop(cmd.w1); break;
            case G_MOVEMEM:
                if ((cmd.w0 & 0xFF) == G_MV_VIEWPORT && resolve(cmd.w1) != 0) {
                    rsp.viewport(resolve(cmd.w1));
                }
                break;
        }
        if (box.empty()) {
            continue;
        }
        // Rectangles join the piece before them when they lie near it. A model is a piece of its own: the shot type's
        // arc, the club's head, the meter's marker and the lie are each within a few pixels of the next and of the
        // meter, so joined they made one piece across the middle. But a model joins the models before it when the
        // middle of one lies inside the other: the banner a shot comes to rest under (ROUGH, ON GREEN, its distance)
        // spins and scales in and out as a black quad and a model for each letter on it, and the letters each placed
        // by where they lay on their own went to the picture's sides while the quad stayed in the middle.
        if (pieces > 0 && !model && !g_pieces[pieces - 1].models && g_pieces[pieces - 1].box.near(box)) {
            g_pieces[pieces - 1].box.add(box);
            g_pieces[pieces - 1].last = i;
            continue;
        }
        if (pieces > 0 && model && g_pieces[pieces - 1].models && g_pieces[pieces - 1].box.overlaps_middle(box)) {
            g_pieces[pieces - 1].box.add(box);
            g_pieces[pieces - 1].last = i;
            continue;
        }
        g_pieces[pieces] = Piece{};
        g_pieces[pieces].box = box;
        g_pieces[pieces].models = model;
        g_pieces[pieces].first = g_pieces[pieces].last = i;
        cmd.piece = pieces++;
    }
    if (pieces == 0) {
        return;
    }
    for (int p = 0; p < pieces; p++) {
        Piece& piece = g_pieces[p];
        float middle = (piece.box.x0 + piece.box.x1) * 0.5f;
        bool across = piece.box.x0 < SCREEN_MIDDLE && piece.box.x1 > SCREEN_MIDDLE;
        // The shot's banner spins in from the left to the middle: placed by where it lay, it went from the left side to
        // the middle halfway, a jump of the picture's extra width at once. It stays where the game draws it throughout.
        if (across || is_banner(piece) ||
            (middle > SCREEN_MIDDLE - MIDDLE_ZONE && middle < SCREEN_MIDDLE + MIDDLE_ZONE)) {
            piece.origin = G_EX_ORIGIN_NONE;
        }
        else {
            piece.origin = middle < SCREEN_MIDDLE ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
        }
    }

    // Write them to the next buffer with the alignments.
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    // The letters' matrices, when they are drawn again, go at the buffer's end.
    uint32_t matrices = buffer + BUFFER_SIZE - MAX_LETTER_MATRICES * MTX_SIZE;
    Writer out{ buffer, matrices };
    GfxCommand c[2];
    gEXEnable(c);
    out.put(c, 1);
    gEXPushScissor(c);
    out.put(c, 1);

    // The game's scissor as it last set it, and whether the one in effect is it widened (for a piece at a side).
    uint32_t sc_mode = G_SC_NON_INTERLACE;
    int sc_ulx = MG_HUD_SCISSOR_ULX, sc_uly = MG_HUD_SCISSOR_ULY, sc_lrx = MG_HUD_SCISSOR_LRX, sc_lry = MG_HUD_SCISSOR_LRY;
    bool sc_wide = false;
    auto set_scissor = [&]() {
        if (sc_wide) {
            scissor(out, sc_mode, sc_ulx, sc_uly, sc_lrx, sc_lry);
        }
        else {
            game_scissor(out, sc_mode, sc_ulx, sc_uly, sc_lrx, sc_lry);
        }
    };
    set_scissor();

    uint32_t viewport_w1 = MG_HUD_VIEWPORT;
    uint32_t rect_origin = G_EX_ORIGIN_NONE;
    uint32_t vp_origin = G_EX_ORIGIN_NONE;
    const Cmd* last_scissor = nullptr;
    // The banner a shot comes to rest under: its quad's matrix (rotation and scale) loaded, then each letter's place on
    // it multiplied on, its quad drawn and the matrix popped. RT64 left to itself blends each matrix's position only
    // while it keeps its pace (rt64_transform_group.h), so as the banner spun and scaled in or out, a letter now and
    // then jumped to its next place in the frames between the game's while the rest were blended, and stood over its
    // neighbour or under the line. So the banner's piece is put in a matrix group, from the matrix before its quad to
    // the next matrix or piece after its last model: its matrices blended whole, their positions always, paired by
    // their order (the quad's first, then the letters'), as golfer.cpp does a golfer's. Its quad starts the group
    // whether its letters are models or, once it has come to rest, rectangles, so RT64 blends it from its last turn
    // to its rest as it did with no group, where a group only while its letters were models left the quad to jump.
    // At rest the game draws the letters as rectangles, level, from the frame the quad comes to its rest, so in the
    // frames between that one and the last the quad turned its last few degrees with the letters already level on it.
    // So on that frame the letters are drawn as the frame before drew them, as models, on the quad at its rest, in
    // place of its rectangles, each moved to where its rectangle lies: RT64 blends them with the quad, and the frame
    // after draws the game's rectangles where the letters are.
    Span spans[MAX_SPANS];
    int span_count = 0;
    // Where the last frame's letters are drawn again: after the triangles of a quad drawn with none (count for nowhere).
    uint32_t replay_at = count;
    // The quad it is drawn on, and up to where the game's rectangles on it (its letters at rest) are left out.
    Box replay_box;
    uint32_t replay_end = 0;
    // Those rectangles, each with its texture's s and t (its G_RDPHALF_1's word), in the order the game draws them.
    struct LetterRect {
        Box box;
        uint32_t st;
        bool used;
    };
    constexpr int MAX_LETTER_RECTS = 64;
    LetterRect letter_rects[MAX_LETTER_RECTS];
    int letter_rect_count = 0;
    for (int p = 0; p < pieces && span_count < MAX_SPANS; p++) {
        const Piece& piece = g_pieces[p];
        if (!is_banner(piece)) {
            continue;
        }
        uint32_t open = piece.first;
        for (uint32_t j = piece.first; j > 0 && piece.first - j < 8; j--) {
            const Cmd& before = g_cmds[j - 1];
            if (before.piece >= 0 || (span_count > 0 && j - 1 < spans[span_count - 1].close)) {
                break;
            }
            if ((before.w0 >> 24) == G_MTX && (before.w0 & G_MTX_PROJECTION) == 0) {
                open = j - 1;
                break;
            }
        }
        uint32_t close = piece.last + 1;
        while (close < count && (g_cmds[close].w0 >> 24) != G_MTX && g_cmds[close].piece < 0) {
            close++;
        }
        spans[span_count++] = Span{ open, close };
        if (piece.last > piece.first) {
            if (!g_letters_kept) {
                g_letters_kept = keep_letters(piece.first + 1, close);
            }
        }
        else if (letters_kept && replay_at == count) {
            replay_at = piece.first + 1;
            while (replay_at < close && is_triangle(g_cmds[replay_at])) {
                replay_at++;
            }
            replay_box = piece.box;
            replay_end = close;
            for (int q = p + 1; q < pieces && !g_pieces[q].models && g_pieces[q].box.inside(replay_box); q++) {
                replay_end = g_pieces[q].last + 1;
            }
            for (uint32_t j = replay_at; j < replay_end && j + 1 < count && letter_rect_count < MAX_LETTER_RECTS;
                 j++) {
                const Cmd& rect = g_cmds[j];
                if (((rect.w0 >> 24) != G_TEXRECT && (rect.w0 >> 24) != G_TEXRECTFLIP) ||
                    (g_cmds[j + 1].w0 >> 24) != G_RDPHALF_1) {
                    continue;
                }
                Box box;
                box.add(float((rect.w1 >> 12) & 0xFFF) / 4.0f, float(rect.w1 & 0xFFF) / 4.0f);
                box.add(float((rect.w0 >> 12) & 0xFFF) / 4.0f, float(rect.w0 & 0xFFF) / 4.0f);
                if (box.inside(replay_box)) {
                    letter_rects[letter_rect_count++] = LetterRect{ box, g_cmds[j + 1].w1, false };
                }
            }
        }
    }
    int next_span = 0;
    bool banner_group = false;
    uint32_t banner_close = 0;

    bool sign_group = false;
    for (uint32_t i = 0; i < count; i++) {
        Cmd& cmd = g_cmds[i];
        uint32_t op = cmd.w0 >> 24;
        if (i == replay_at) {
            for (uint32_t m = 0; m < g_letter_mtx_count; m++) {
                for (uint32_t w = 0; w < MTX_SIZE / 4; w++) {
                    wr_w(matrices + m * MTX_SIZE + w * 4, g_letter_mtx[m][w]);
                }
            }
            // Each letter moved to its rectangle, the one whose texture starts where the letter's first vertex's does
            // (the game spaces the letters evenly as they spin and by their widths at rest), so the frames between
            // blend the spacing too. Where each lands is followed as the RSP would, from this frame's state there.
            Rsp rsp_at;
            rsp_at.viewport(MG_HUD_VIEWPORT);
            for (uint32_t j = 0; j < replay_at; j++) {
                uint32_t jop = g_cmds[j].w0 >> 24;
                if (jop == G_MTX) {
                    rsp_at.matrix(g_cmds[j].w0, g_cmds[j].w1);
                }
                else if (jop == G_POPMTX) {
                    rsp_at.pop(g_cmds[j].w1);
                }
                else if (jop == G_DL) {
                    rsp_at.call(resolve(g_cmds[j].w1), 0);
                }
                else if (jop == G_MOVEMEM && (g_cmds[j].w0 & 0xFF) == G_MV_VIEWPORT && resolve(g_cmds[j].w1) != 0) {
                    rsp_at.viewport(resolve(g_cmds[j].w1));
                }
            }
            int letter_mtx = -1;
            Box letter_box;
            for (uint32_t k = 0; k < g_letter_count; k++) {
                uint32_t w0 = g_letter_cmds[k][0], w1 = g_letter_cmds[k][1];
                uint32_t kop = w0 >> 24;
                if (kop == G_MTX) {
                    letter_mtx = g_letter_mtx_of[k];
                    rsp_at.matrix(w0, (matrices + letter_mtx * MTX_SIZE) & 0x1FFFFFFF);
                }
                else if (kop == G_POPMTX) {
                    rsp_at.pop(w1);
                    letter_mtx = -1;
                }
                else if (kop == G_VTX) {
                    letter_box = rsp_at.vertices(w0, w1);
                }
                else if (w0 == 0x02140000 && letter_mtx >= 0 && rsp_at.depth > 0 && !letter_box.empty()) {
                    for (int r = 0; r < letter_rect_count; r++) {
                        LetterRect& rect = letter_rects[r];
                        if (rect.used || rect.st != w1) {
                            continue;
                        }
                        rect.used = true;
                        float dx = rect.box.x0 - letter_box.x0, dy = rect.box.y0 - letter_box.y0;
                        // The screen's pixels for a unit of the letter's place: its quad's matrix, the projection
                        // and the viewport (y down).
                        Mat m = mul(rsp_at.mv[rsp_at.depth - 1], rsp_at.proj);
                        float a = m.m[0][0] * rsp_at.vp_scale_x, b = m.m[1][0] * rsp_at.vp_scale_x;
                        float c2 = -m.m[0][1] * rsp_at.vp_scale_y, d = -m.m[1][1] * rsp_at.vp_scale_y;
                        float det = a * d - b * c2;
                        if (dx * dx + dy * dy < 24.0f * 24.0f && (det > 1e-6f || det < -1e-6f)) {
                            uint32_t addr = matrices + letter_mtx * MTX_SIZE;
                            add_mtx(addr, 3, 0, (d * dx - b * dy) / det);
                            add_mtx(addr, 3, 1, (a * dy - c2 * dx) / det);
                        }
                        break;
                    }
                    letter_box = Box{};
                }
            }
            for (uint32_t k = 0; k < g_letter_count; k++) {
                int m = g_letter_mtx_of[k];
                out.put(g_letter_cmds[k][0], m < 0 ? g_letter_cmds[k][1] : (matrices + m * MTX_SIZE) & 0x1FFFFFFF);
            }
        }
        if (banner_group && i >= banner_close) {
            gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
            out.put(c, 1);
            banner_group = false;
        }
        if (i >= replay_at && i < replay_end && (op == G_TEXRECT || op == G_TEXRECTFLIP)) {
            Box rect;
            rect.add(float((cmd.w1 >> 12) & 0xFFF) / 4.0f, float(cmd.w1 & 0xFFF) / 4.0f);
            rect.add(float((cmd.w0 >> 12) & 0xFFF) / 4.0f, float(cmd.w0 & 0xFFF) / 4.0f);
            if (rect.inside(replay_box)) {
                // With its texture coordinates and steps.
                while (i + 1 < count && ((g_cmds[i + 1].w0 >> 24) == G_RDPHALF_1 ||
                                         (g_cmds[i + 1].w0 >> 24) == G_RDPHALF_2)) {
                    i++;
                }
                continue;
            }
        }
        if (next_span < span_count && i >= spans[next_span].open) {
            if (sign_group) {
                gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
                out.put(c, 1);
                sign_group = false;
            }
            gEXMatrixGroupSimple(c, BANNER_GROUP_ID, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                                 G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_SKIP,
                                 G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP,
                                 G_EX_COMPONENT_AUTO);
            out.put(c, 2);
            banner_group = true;
            banner_close = spans[next_span].close;
            next_span++;
        }
        // The sign's group ends before the next load, which would take it.
        if (sign_group && (op == G_VTX || op == G_DL)) {
            gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
            out.put(c, 1);
            sign_group = false;
        }
        if (cmd.piece >= 0) {
            const Piece& piece = g_pieces[cmd.piece];
            if ((piece.origin != G_EX_ORIGIN_NONE) != sc_wide) {
                sc_wide = !sc_wide;
                set_scissor();
            }
            if (piece.origin != rect_origin) {
                rect_align(out, piece.origin);
                rect_origin = piece.origin;
            }
            if (piece.models && piece.origin != vp_origin) {
                viewport_align(out, piece.origin, viewport_w1);
                vp_origin = piece.origin;
            }
        }
        if (op == G_VTX && cmd.w0 == SIGN_VTX_W0 && resolve(cmd.w1) - SIGN_VERTICES < SIGN_VERTICES_SIZE) {
            // Its vertices interpolated, everything else as RT64 does with no group (rt64_transform_group.h).
            gEXMatrixGroupDecomposed(c, SIGN_GROUP_ID, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_AUTO,
                                     G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO,
                                     G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR, G_EX_EDIT_NONE,
                                     G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
            out.put(c, 2);
            sign_group = true;
        }
        if (op == G_SETSCISSOR) {
            sc_mode = (cmd.w1 >> 24) & 0x3;
            sc_ulx = ((cmd.w0 >> 12) & 0xFFF) / 4;
            sc_uly = (cmd.w0 & 0xFFF) / 4;
            sc_lrx = ((cmd.w1 >> 12) & 0xFFF) / 4;
            sc_lry = (cmd.w1 & 0xFFF) / 4;
            set_scissor();
            last_scissor = &cmd;
            continue;
        }
        if (op == G_MOVEMEM && (cmd.w0 & 0xFF) == G_MV_VIEWPORT) {
            viewport_w1 = cmd.w1;
        }
        if (op == G_TEXRECT && !sc_wide && i + 2 < count &&
            spanning_rect_from_left(cmd, g_cmds[i + 1], g_cmds[i + 2], sc_ulx, sc_uly, sc_lrx, sc_lry)) {
            game_scissor(out, sc_mode, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            for (int k = 0; k < 3; k++) {
                out.put(g_cmds[i + k].w0, g_cmds[i + k].w1);
            }
            i += 2;
            set_scissor();
            continue;
        }
        out.put(cmd.w0, cmd.w1);
    }
    if (sign_group || banner_group) {
        gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
        out.put(c, 1);
    }

    gEXSetRectAlign(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    out.put(c, 2);
    if (vp_origin != G_EX_ORIGIN_NONE) {
        viewport_align(out, G_EX_ORIGIN_NONE, viewport_w1);
    }
    gEXPopScissor(c);
    out.put(c, 1);
    if (last_scissor != nullptr) {
        out.put(last_scissor->w0, last_scissor->w1);
    }
    out.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), end & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    // The game's first command becomes the branch to the buffer.
    wr_w(start, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
    wr_w(start + 4, buffer & 0x1FFFFFFF);
}

} // namespace

extern "C" void mg_hud_begin(uint8_t* rdram) {
    g_rdram = rdram;
    g_start = rd_w(MG_GFX_HEAD);
    g_open = true;
}

extern "C" void mg_hud_end(uint8_t* rdram) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    g_full = mg_border_fills(rdram);
    anchor(g_start, rd_w(MG_GFX_HEAD));
}
