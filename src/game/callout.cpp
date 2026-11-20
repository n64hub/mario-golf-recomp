// The big rainbow words a hole or a shot ends on (HOLE-IN-ONE!, BIRDIE, PAR, NICE SHOT!, OB! and the rest) in the
// frames RT64 draws between the game's: each letter blended from where it was to where it goes, whole.
//
// The game draws such a word a letter at a time, each letter a textured quad: the four vertices at 0x80224130, the same
// for every letter, under a modelview matrix loaded for the word and one multiplied onto it that places, sizes and
// squashes that letter. In the frame's display list each letter is the G_SETTIMG of its texture and its tile's loads,
// then four commands: the word's matrix loaded (G_MTX, modelview, load, no push), the letter's multiplied (no push), the
// G_VTX of the four and a G_TRI2. The word slides in, squashes and bounces, its letters spread and drawn together.
//
// RT64 left to itself pairs each of a frame's matrices with one of the frame before's among the draws alike, and every
// letter is alike to it (the same quad, combiner and modes, the texture aside): as the word squashed and bounced it
// paired a letter with its neighbour, or with none, and blends a matrix's position only while it keeps its pace, so in
// the frames between the game's, letters went missing, stood doubled or a strip of the word sat out of place, where the
// game's own frames drew the word whole (issue 154: at 120 Hz three frames in four are RT64's, and the word stepped).
//
// So each letter's four commands are put in an RT64 matrix group of its own (its extended GBI), its matrices blended
// element by element, not split, their positions always, as golfer.cpp does a golfer's joints. The group's ID is made
// from the letter's texture, which the game keeps at the same place for the same letter from frame to frame, and how
// many letters before it in the frame drew from that texture, so a letter pairs only with itself in the frame before.
//
// nuGfxTaskStart sends each frame's display list, and the hook there calls mg_callouts with it (us.toml). Each letter's
// first command is replaced by a branch to a buffer of this file's in RDRAM, above the 4 MB the game uses, which opens
// the group, holds its four commands, closes the group and branches back to the command after them.

#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace {

// F3DEX2's commands this reads and writes (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_SETTIMG = 0xFD, G_TRI1 = 0x05, G_TRI2 = 0x06;
constexpr uint32_t G_DL_NOPUSH = 1;
// G_MTX's parameter byte for a modelview matrix.
constexpr uint32_t G_MTX_MODELVIEW = 0x00;
// A letter: the word's matrix loaded and the letter's multiplied (F3DEX2's G_MTX, its parameters xor G_MTX_PUSH, so
// neither pushes), and the G_VTX of its four vertices.
constexpr uint32_t MTX_LOAD = 0xDA380003, MTX_MUL = 0xDA380001;
constexpr uint32_t QUAD_VTX_W0 = 0x01004008, QUAD_VTX_W1 = 0x80224130;
constexpr uint32_t LETTER_COMMANDS = 4;
// How far before a letter its texture is set, at most: its G_SETTIMG and its tile's loads, seven commands.
constexpr uint32_t TEXTURE_LOOKBACK = 12;

// The other matrix groups' IDs these must not take: src/game/backdrop.cpp's sky, src/game/hud.cpp's sign and banner.
constexpr uint32_t OTHER_GROUP_IDS[] = { 0x4D47534B, 0x4D475347, 0x4D474252 };

// Above iris.cpp's buffers (which end at 0x80781000), below billboard.cpp's. Twelve letters a word, and the renderer may
// read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80788000;
constexpr uint32_t BUFFER_SIZE = 0x50;
constexpr uint32_t BUFFER_COUNT = 256;
// The commands read from a list, at most.
constexpr uint32_t MAX_COMMANDS = 0x4000;
// Letters a frame, at most, whose textures are counted.
constexpr uint32_t MAX_LETTERS = 64;

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

bool is_letter(uint32_t addr) {
    uint32_t tri = rd_w(addr + 24) >> 24;
    return rd_w(addr) == MTX_LOAD && rd_w(addr + 8) == MTX_MUL && rd_w(addr + 16) == QUAD_VTX_W0 &&
           rd_w(addr + 20) == QUAD_VTX_W1 && (tri == G_TRI2 || tri == G_TRI1);
}

// The letter's texture: the last G_SETTIMG before it, 0 if there is none close by.
uint32_t texture(uint32_t list, uint32_t addr) {
    for (uint32_t i = 1; i <= TEXTURE_LOOKBACK && addr - i * 8 >= list; i++) {
        uint32_t w0 = rd_w(addr - i * 8);
        if ((w0 >> 24) == G_SETTIMG) {
            return rd_w(addr - i * 8 + 4);
        }
    }
    return 0;
}

// The group's ID: the texture and how many letters before it drew from it this frame, mixed, away from the IDs RT64
// reserves and the other groups'.
uint32_t group_id(uint32_t tex, uint32_t repeat) {
    uint32_t id = (0x43414C4Cu ^ tex) * 0x9E3779B1u;
    id ^= id >> 15;
    id = (id ^ repeat) * 0x85EBCA6Bu;
    id ^= id >> 13;
    for (uint32_t other : OTHER_GROUP_IDS) {
        if (id == other) {
            id ^= 0x5A5A5A5Au;
        }
    }
    if (id == G_EX_ID_IGNORE || id == G_EX_ID_AUTO) {
        id ^= 0x5A5A5A5Au;
    }
    return id;
}

void group(uint32_t addr, uint32_t id) {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    Writer out{ buffer, buffer + BUFFER_SIZE };
    GfxCommand c[2];
    gEXEnable(c);
    out.put(c, 1);
    // Blended whole, its position always, in order; the vertices and texture coordinates as RT64 does with no group.
    gEXMatrixGroupSimple(c, id, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
                         G_EX_COMPONENT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO, G_EX_ORDER_LINEAR,
                         G_EX_EDIT_NONE, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_AUTO);
    out.put(c, 2);
    for (uint32_t i = 0; i < LETTER_COMMANDS; i++) {
        out.put(rd_w(addr + i * 8), rd_w(addr + i * 8 + 4));
    }
    gEXPopMatrixGroup(c, G_MTX_MODELVIEW);
    out.put(c, 1);
    out.put((uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16), (addr + LETTER_COMMANDS * 8) & 0x1FFFFFFF);
    if (!out.ok) {
        return;
    }
    wr_w(addr, (uint32_t(G_DL) << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

} // namespace

extern "C" void mg_callouts(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) != 0) {
        return;
    }
    uint32_t textures[MAX_LETTERS];
    uint32_t letters = 0;
    for (uint32_t i = 0, addr = list; i < MAX_COMMANDS; i++, addr += 8) {
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_ENDDL) {
            break;
        }
        if (!is_letter(addr)) {
            continue;
        }
        uint32_t tex = texture(list, addr);
        if (tex != 0 && letters < MAX_LETTERS) {
            uint32_t repeat = 0;
            for (uint32_t j = 0; j < letters; j++) {
                repeat += textures[j] == tex;
            }
            textures[letters++] = tex;
            group(addr, group_id(tex, repeat));
        }
        addr += (LETTER_COMMANDS - 1) * 8;
        i += LETTER_COMMANDS - 1;
    }
}
