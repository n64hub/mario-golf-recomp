// The old shot of a transition kept at the game's 4:3 shape in a wide picture (RT64's Aspect Ratio Expand).
//
// As the intro cuts between its shots, and as a shot's camera cuts to a view from the green while the ball flies, the
// game dissolves from the old shot to the new one, the HUD included. It copies its last frame of the old shot out of the framebuffer into a buffer of its own (304x240,
// 16-bit) and draws that copy over the new shot, fading it out, as 38 texture rectangles of 6 rows each from x 8 to 312
// (func_8009232C, called by the transition drawer func_800957F0 for its transition types 8 and 9). The copy is what the
// game read back from RAM, and RT64 renders that at the game's own 320x240 for it, so it holds the 4:3 frame and none of
// the wide picture's sides. Each strip spans the screen's scissor, and RT64 stretches a rectangle that spans it across
// the wide picture, so the old shot was drawn stretched sideways, jumping wider as the dissolve began.
//
// us.toml hooks the drawer's one call of func_8009232C on both sides: mg_card_strips_begin adds RT64's
// gEXSetRectAspect(G_EX_ASPECT_ADJUST) to the frame's display list, so its rectangles keep their 4:3 shape in the
// middle of the picture, and mg_card_strips_end puts G_EX_ASPECT_AUTO back. The old shot fades out in the middle while
// the new one fills the sides. The drawer's other transitions need nothing: its tiles (type 10) are each narrower than
// the screen, and the card that turns over to a course's next shot (type 2) is a model, both kept at 4:3 by RT64.

#include <cstdint>

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace {

// The word holding the frame's display list's head, where the game adds its next command.
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;

uint32_t rd_w(uint8_t* rdram, uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF));
}

void wr_w(uint8_t* rdram, uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF)) = value;
}

// Adds a command at the list's head, as the game adds its own.
void put(uint8_t* rdram, const GfxCommand& c) {
    uint32_t head = rd_w(rdram, MG_GFX_HEAD);
    wr_w(rdram, head, c.values.word0);
    wr_w(rdram, head + 4, c.values.word1);
    wr_w(rdram, MG_GFX_HEAD, head + 8);
}

} // namespace

extern "C" void mg_card_strips_begin(uint8_t* rdram) {
    GfxCommand c[1];
    gEXEnable(c);
    put(rdram, c[0]);
    gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
    put(rdram, c[0]);
}

extern "C" void mg_card_strips_end(uint8_t* rdram) {
    GfxCommand c[1];
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    put(rdram, c[0]);
}
