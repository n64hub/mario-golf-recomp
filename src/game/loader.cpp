// Ring Shot's stage select: its wooden slats shut over the course's load, and open again once it is in.
//
// A press of left or right there (the stage at 0x801B6098, the game mode at 0x801B608C 9) sets overlay 13's slats
// closing (func_ovl13_80242384(0), at 0x80060BB0) and starts the hole loader thread (func_80069D08), which loads the
// next course; once it is done (0x800C3FA8 1), the main thread sets them opening (func_ovl13_80242384(1), at
// 0x8006093C). The board's HUD (func_ovl13_802427E8, drawn while 0x80243EA8 is 1) moves them each frame it draws:
// func_ovl13_802423A4 steps a word at 0x80243EE0 by 0x18 towards 0x1E0, shut, while 0x80243EE4 is 0, and back to 0,
// open, while it is 1, the slats drawn from the top row down by how far it has gone. On a console the loader, at a
// priority above the main thread's, decompresses the course for about a second while the graphics thread draws, so the
// slats close, stay shut over the load and open on the new course. Here a game thread runs until it yields, so the
// loader is done before the next frame and the slats turned to open after one step: two corner pieces at the top, for
// a frame.
//
// So us.toml hooks the loader at the end of its loads (before 0x80069ED8): while the slats are closing it yields until
// they are shut, then lets the load end, and they open as soon as they have shut. It gives up after
// MG_SLATS_MAX_RETRACES, should the HUD stop drawing them, so the load always ends. Every other load the thread makes
// (a hole in every mode, the stage select's first) has no slats closing and goes on at once.

#include <cstdint>

namespace {

// The retrace count, which the graphics callback advances (30 a second).
constexpr uint32_t MG_RETRACES = 0x800B67F0;
// The game mode: 9 Ring Shot's stage select, with overlay 13 at 0x80241800.
constexpr uint32_t MG_GAME_MODE = 0x801B608C;
constexpr uint32_t MG_MODE_RING_SHOT_SELECT = 9;
// Overlay 13's HUD (1 the stage select's board, which draws the slats), the slats' way (0 closing, 1 opening) and how
// far they have closed (0 open, 0x1E0 shut).
constexpr uint32_t MG_RING_HUD = 0x80243EA8;
constexpr uint32_t MG_SLATS_WAY = 0x80243EE4;
constexpr uint32_t MG_SLATS_SHUT = 0x80243EE0;
constexpr uint32_t MG_SLATS_SHUT_AT = 0x1E0;
// 20 frames close them; a wait twice as long and more is the HUD no longer drawing them.
constexpr uint32_t MG_SLATS_MAX_RETRACES = 60;

uint32_t rd_w(uint8_t* rdram, uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF));
}

} // namespace

extern "C" void yield_self(uint8_t* rdram);

// The hook at the end of the loader's loads.
extern "C" void mg_load_end(uint8_t* rdram) {
    if (rd_w(rdram, MG_GAME_MODE) != MG_MODE_RING_SHOT_SELECT || rd_w(rdram, MG_RING_HUD) != 1 ||
        rd_w(rdram, MG_SLATS_WAY) != 0) {
        return;
    }
    uint32_t from = rd_w(rdram, MG_RETRACES);
    while ((int32_t)rd_w(rdram, MG_SLATS_SHUT) < (int32_t)MG_SLATS_SHUT_AT &&
           rd_w(rdram, MG_RETRACES) - from < MG_SLATS_MAX_RETRACES) {
        yield_self(rdram);
    }
}
