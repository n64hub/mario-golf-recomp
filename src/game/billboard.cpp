// The course's billboards in the frames RT64 draws between the game's: each paired only with one of its own size.
//
// The trees and the Bomb-ombs around a course are one billboard: a quad of four vertices that never change
// (0x800B6200), turned to face the camera and scaled to what it shows, a tree 34 and a Bomb-omb 10, under its texture.
// Each is eight commands in the frame's display list: a G_MTX pushed and multiplied that puts it in its place, a G_MTX
// multiplied that turns it to the camera (one matrix every billboard of the view shares), a G_MTX multiplied that
// scales it, a pipe sync, the G_VTX of the four, a G_TRI2, a pipe sync and the G_POPMTX.
//
// RT64 pairs each of a frame's matrices with one of the frame before's to draw the frames between, among the draws
// alike: the same combiner, render modes, geometry mode and number of triangles, not the same texture, so a tree and
// a Bomb-omb are alike to it, and the pair it takes is the one closest in place, turn and size. Where a billboard came
// into the view as another left it, it could be paired with that one, and as RT64 blends a matrix's scale but draws its
// position at once where it jumps, it was drawn between the frames at its own place and halfway to the other's size:
// on Mini-Golf's flyover of Luigi's Garden's first hole, a Bomb-omb coming into view at the top of the picture three
// times as big as it is for a frame, halfway to a tree's size.
//
// So each billboard is put in an RT64 matrix group (its extended GBI) whose ID is made from its scale, everything else as
// RT64 does with no group: RT64 pairs only draws of the same group, so a Bomb-omb is paired only with a Bomb-omb and a
// tree with a tree, and one that came into view as no other of its size left is drawn as the game draws it.
//
// nuGfxTaskStart sends each frame's display list, and the hook there calls mg_billboards with it (us.toml) before iris.cpp
// and screen.cpp read it. Each billboard's first command is replaced by a branch to a buffer of this file's in RDRAM,
// above the 4 MB the game uses, which opens the group, holds its eight commands, closes the group after its G_POPMTX and
// branches back to the command after it.

#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace {

// F3DEX2's commands this reads and writes (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_VTX = 0x01, G_TRI2 = 0x06, G_RDPPIPESYNC = 0xE7;
constexpr uint32_t G_DL_NOPUSH = 1;
// G_MTX's parameter byte for a modelview matrix.
constexpr uint32_t G_MTX_MODELVIEW = 0x00;
// A billboard's matrices: its place pushed and multiplied, then its turn and its scale multiplied (F3DEX2's G_MTX, its
// parameters xor G_MTX_PUSH), and the G_POPMTX of one matrix that ends it.
constexpr uint32_t MTX_PUSH_MUL = 0xDA380000, MTX_MUL = 0xDA380001;
constexpr uint32_t POPMTX_W0 = 0xD8380002, POPMTX_W1 = 0x40;
// The quad: four vertices from 0x800B6200.
constexpr uint32_t QUAD_VTX_W0 = (uint32_t(G_VTX) << 24) | 0x004008, QUAD_VTX_W1 = 0x800B6200;
// The commands of a billboard.
constexpr uint32_t BILLBOARD_COMMANDS = 8;

// The other matrix groups' IDs these must not take (src/game/backdrop.cpp's sky).
constexpr uint32_t SKY_GROUP_ID = 0x4D47534B;

// Above iris.cpp's buffers. About 80 billboards a frame, and the renderer may read a list a frame or more after the game
// sends it.
constexpr uint32_t BUFFER_BASE = 0x80790000;
constexpr uint32_t BUFFER_SIZE = 0x80;
constexpr uint32_t BUFFER_COUNT = 1024;
// The commands read from a list, at most.
constexpr uint32_t MAX_COMMANDS = 0x4000;

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

// A matrix's address in RDRAM from a G_MTX's: the game gives billboards' as KSEG0 addresses or physical ones (segment
// 0). 0 for any other segment.
uint32_t matrix_addr(uint32_t w1) {
    uint32_t top = w1 >> 24;
    if (top == 0x80) {
        return w1;
    }
    if (top == 0x00) {
        return w1 | 0x80000000u;
    }
    return 0;
}

bool is_billboard(uint32_t addr) {
    return rd_w(addr) == MTX_PUSH_MUL && rd_w(addr + 8) == MTX_MUL && rd_w(addr + 16) == MTX_MUL &&
           (rd_w(addr + 24) >> 24) == G_RDPPIPESYNC && rd_w(addr + 32) == QUAD_VTX_W0 &&
           rd_w(addr + 36) == QUAD_VTX_W1 && (rd_w(addr + 40) >> 24) == G_TRI2 &&
           (rd_w(addr + 48) >> 24) == G_RDPPIPESYNC && rd_w(addr + 56) == POPMTX_W0 && rd_w(addr + 60) == POPMTX_W1 &&
           matrix_addr(rd_w(addr + 20)) != 0;
}

// The group's ID: the scale matrix's whole 3x3 (its integer and fraction halves), mixed, away from the IDs RT64
// reserves and the sky's.
uint32_t group_id(uint32_t scale) {
    // The 3x3's words: rows 0 to 2 of the integer halves, then of the fraction halves.
    static constexpr uint32_t OFFSETS[] = { 0, 4, 8, 12, 16, 20, 32, 36, 40, 44, 48, 52 };
    uint32_t id = 0x42424C44u;
    for (uint32_t offset : OFFSETS) {
        id = (id ^ rd_w(scale + offset)) * 0x9E3779B1u;
        id ^= id >> 15;
    }
    if (id == G_EX_ID_IGNORE || id == G_EX_ID_AUTO || id == SKY_GROUP_ID) {
        id ^= 0x5A5A5A5Au;
    }
    return id;
}

void group(uint32_t addr) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[2];
    gEXEnable(c);
    out.put(c, 1);
    // Everything as RT64 does with no group (rt64_transform_group.h), but the ID.
    gEXMatrixGroupDecomposed(c, group_id(matrix_addr(rd_w(addr + 20))), G_EX_PUSH, G_MTX_MODELVIEW,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO, G_EX_COMPONENT_AUTO,
                             G_EX_COMPONENT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_AUTO,
                             G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    out.put(c, 2);
    for (uint32_t i = 0; i < BILLBOARD_COMMANDS; i++) {
        out.put(rd_w(addr + i * 8), rd_w(addr + i * 8 + 4));
    }
    gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
    out.put(c, 1);
    out.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), (addr + BILLBOARD_COMMANDS * 8) & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    wr_w(addr, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

} // namespace

extern "C" void mg_billboards(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) != 0) {
        return;
    }
    for (uint32_t i = 0, addr = list; i < MAX_COMMANDS; i++, addr += 8) {
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_ENDDL) {
            break;
        }
        if (is_billboard(addr)) {
            group(addr);
            addr += (BILLBOARD_COMMANDS - 1) * 8;
            i += BILLBOARD_COMMANDS - 1;
        }
    }
}
