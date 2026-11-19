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
// the meter with its marks, and each model is a piece of its own. A piece across the middle of the screen, or near it,
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
constexpr uint32_t G_MTX_PUSH = 1, G_MTX_LOAD = 2, G_MTX_PROJECTION = 4;
// gSPViewport's first word.
constexpr uint32_t G_VIEWPORT_W0 = 0xDC080008;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The full-screen viewport the HUD draws in (the game's own, set before the HUD), re-sent when its alignment changes.
constexpr uint32_t MG_HUD_VIEWPORT = 0x800B77A0;
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
};

Cmd g_cmds[MAX_COMMANDS];
Piece g_pieces[MAX_COMMANDS];

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
// On a hole drawn at the frame's full height (border.cpp) the border's top and bottom are the screen's.
void game_scissor(Writer& out, uint32_t mode, int ulx, int uly, int lrx, int lry) {
    GfxCommand c[2];
    if (g_full && uly == MG_HUD_SCISSOR_ULY && lry == MG_HUD_SCISSOR_LRY) {
        uly = 0;
        lry = SCREEN_HEIGHT;
    }
    gEXSetScissor(c, mode, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, ulx, uly, lrx, lry);
    out.put(c, 2);
}

// RT64 places the left edge of a rectangle it stretches across the picture (one that spans the scissor's width) a pixel
// or two right of the stretched scissor's, which is the 3D's border: Lakitu Mode's letterbox bars, texture rectangles
// from the scissor's left to its right (8 to 312), left a column of the course showing at their left ends. Such a
// rectangle starts at the screen's left instead, its texture moved with it, and the scissor cuts it at the 3D's border.
// Its texture coordinates are the next command's (G_RDPHALF_1: s and t), its steps the one after (G_RDPHALF_2).
// On a hole drawn at the frame's full height (border.cpp), such a rectangle at the border's top or bottom (a bar) runs on
// to the screen's, its texture moved with its top, or the course showed above the top bar and below the bottom one.
void spanning_rect_from_left(Cmd& rect, Cmd& half1, const Cmd& half2, int sc_ulx, int sc_uly, int sc_lrx, int sc_lry) {
    uint32_t ulx = (rect.w1 >> 12) & 0xFFF, lrx = (rect.w0 >> 12) & 0xFFF;
    uint32_t uly = rect.w1 & 0xFFF, lry = rect.w0 & 0xFFF;
    if (ulx == 0 || ulx != uint32_t(sc_ulx) * 4 || lrx != uint32_t(sc_lrx) * 4 || (half1.w0 >> 24) != G_RDPHALF_1 ||
        (half2.w0 >> 24) != G_RDPHALF_2) {
        return;
    }
    // s and t are 10.5 fixed point, dsdx and dtdy 5.10, ulx and uly in quarter pixels.
    int32_t s = int16_t(half1.w1 >> 16), t = int16_t(half1.w1 & 0xFFFF);
    int32_t dsdx = int16_t(half2.w1 >> 16), dtdy = int16_t(half2.w1 & 0xFFFF);
    s -= int32_t(ulx) * dsdx / 128;
    rect.w1 &= ~(0xFFFu << 12);
    if (g_full && sc_uly == MG_HUD_SCISSOR_ULY && sc_lry == MG_HUD_SCISSOR_LRY) {
        if (uly == uint32_t(sc_uly) * 4) {
            t -= int32_t(uly) * dtdy / 128;
            rect.w1 &= ~0xFFFu;
        }
        if (lry == uint32_t(sc_lry) * 4) {
            rect.w0 = (rect.w0 & ~0xFFFu) | uint32_t(SCREEN_HEIGHT * 4);
        }
    }
    half1.w1 = (uint32_t(uint16_t(s)) << 16) | uint16_t(t);
}

void anchor(uint32_t start, uint32_t end) {
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
        // meter, so joined they made one piece across the middle.
        if (pieces > 0 && !model && !g_pieces[pieces - 1].models && g_pieces[pieces - 1].box.near(box)) {
            g_pieces[pieces - 1].box.add(box);
            continue;
        }
        g_pieces[pieces] = Piece{};
        g_pieces[pieces].box = box;
        g_pieces[pieces].models = model;
        cmd.piece = pieces++;
    }
    if (pieces == 0) {
        return;
    }
    for (int p = 0; p < pieces; p++) {
        Piece& piece = g_pieces[p];
        float middle = (piece.box.x0 + piece.box.x1) * 0.5f;
        bool across = piece.box.x0 < SCREEN_MIDDLE && piece.box.x1 > SCREEN_MIDDLE;
        if (across || (middle > SCREEN_MIDDLE - MIDDLE_ZONE && middle < SCREEN_MIDDLE + MIDDLE_ZONE)) {
            piece.origin = G_EX_ORIGIN_NONE;
        }
        else {
            piece.origin = middle < SCREEN_MIDDLE ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
        }
    }

    // Write them to the next buffer with the alignments.
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
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
    for (uint32_t i = 0; i < count; i++) {
        Cmd& cmd = g_cmds[i];
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
        uint32_t op = cmd.w0 >> 24;
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
        if (op == G_TEXRECT && !sc_wide && i + 2 < count) {
            spanning_rect_from_left(cmd, g_cmds[i + 1], g_cmds[i + 2], sc_ulx, sc_uly, sc_lrx, sc_lry);
        }
        out.put(cmd.w0, cmd.w1);
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
