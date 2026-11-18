// The golfers' joints in the frames RT64 draws between the game's: each joint blended from where it was to where it
// goes, whole.
//
// func_80057FFC draws a golfer (the Play Modes screen's, character select's, the course's): it points segment 2 at the
// model's joint matrices, loads the matrix that places it, calls the model's body display list (the model's struct,
// at 0x801F43F0 plus 0x18C a slot, holds it at +0x64), which loads each joint's matrix from segment 2 and draws that
// part, and ends with a G_POPMTX and a pipe sync. RT64 left to itself blends a matrix in two ways that broke Wario's
// fist-shake on the Play Modes screen. It splits each matrix into a rotation, a scale and a skew and blends those, and
// for his raised upper arm between two of the game's frames with it up at his face that took a half turn: in every
// frame drawn between, the sleeve swung down to his belly and back while his glove stayed up. And it blends a matrix's
// position only while it moves at about the pace it did the frame before, so as a new move starts, the upper arm,
// jumping further than it was moving, was drawn at once at its new place, his belly, while the glove beside it was
// still blended from his face.
//
// So the body's call is wrapped in an RT64 matrix group of the golfer's own (its extended GBI): its matrices blended
// element by element, not split, their positions always, and paired by their order, the first joint's matrix with the
// first's of the last frame and so on, as the model loads them in the same order every frame. Its ID is made from the
// model's struct and its body list, so a golfer pairs only with the same golfer drawn the frame before, never with one
// drawn in another slot or a new model loaded into it.
//
// us.toml hooks the function where it stores the display list's head back (0x80058AA4), with the head and the model's
// struct in hand. If the last three commands it wrote are the body's call, the G_POPMTX and the pipe sync, the call is
// replaced by a branch to a buffer of this file's in RDRAM above the 4 MB the game uses, which opens the group, calls
// the body as the game did, closes the group and branches back to the G_POPMTX. Anything else is left as it was.

#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace {

// F3DEX2's commands this reads and writes (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_POPMTX = 0xD8, G_RDPPIPESYNC = 0xE7;
constexpr uint32_t G_DL_PUSH = 0, G_DL_NOPUSH = 1;
// G_MTX's parameter byte for a modelview matrix.
constexpr uint32_t G_MTX_MODELVIEW = 0x00;
// The G_POPMTX func_80057FFC writes after the body: one modelview matrix (64 bytes).
constexpr uint32_t POPMTX_W0 = (uint32_t(G_POPMTX) << 24) | 0x380002, POPMTX_W1 = 0x40;

// Where the model's struct keeps its body display list.
constexpr uint32_t MODEL_BODY_DL = 0x64;
// The other matrix groups' IDs this one must not take (src/game/backdrop.cpp's sky).
constexpr uint32_t SKY_GROUP_ID = 0x4D47534B;

// Above backdrop.cpp's buffers, below screen.cpp's. Up to five golfers a frame, and the renderer may read a list a frame
// or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80750000;
constexpr uint32_t BUFFER_SIZE = 0x40;
constexpr uint32_t BUFFER_COUNT = 1024;

uint8_t* g_rdram;
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

// The golfer's group ID: the model's struct and its body list, mixed, away from the IDs RT64 reserves and the sky's.
uint32_t group_id(uint32_t model, uint32_t body) {
    uint32_t id = model * 0x9E3779B1u ^ body * 0x85EBCA77u;
    id ^= id >> 15;
    if (id == G_EX_ID_IGNORE || id == G_EX_ID_AUTO || id == SKY_GROUP_ID) {
        id ^= 0x5A5A5A5Au;
    }
    return id;
}

} // namespace

// func_80057FFC's end: head is the display list's head it is about to store, model its golfer's struct (0 when it drew
// nothing).
extern "C" void mg_golfer_end(uint8_t* rdram, uint32_t head, uint32_t model) {
    g_rdram = rdram;
    if (model == 0 || (head & 7) != 0 || head < 0x80000000u + 0x18) {
        return;
    }
    uint32_t call = head - 0x18, pop = head - 0x10, sync = head - 0x8;
    uint32_t body = rd_w(model + MODEL_BODY_DL);
    if (rd_w(call) != (uint32_t(G_DL) << 24) || rd_w(call + 4) != body || rd_w(pop) != POPMTX_W0 ||
        rd_w(pop + 4) != POPMTX_W1 || (rd_w(sync) >> 24) != G_RDPPIPESYNC) {
        return;
    }

    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[2];
    gEXEnable(c);
    out.put(c, 1);
    // Blended whole, its position always, in order; the vertices and texture coordinates as RT64 does with no group.
    gEXMatrixGroupSimple(c, group_id(model, body), G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                         G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO,
                         G_EX_ORDER_LINEAR, G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    out.put(c, 2);
    out.put((uint32_t(G_DL) << 24) | (G_DL_PUSH << 16), body);
    gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
    out.put(c, 1);
    out.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), pop & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    wr_w(call, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
    wr_w(call + 4, buffer & 0x1FFFFFFF);
}
