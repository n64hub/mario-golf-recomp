// The old shot of a transition kept at the game's 4:3 shape in a wide picture (RT64's Aspect Ratio Expand).
//
// As the intro cuts between its shots (transition type 9), and as a shot's camera cuts to a view from the green while
// the ball flies (type 8), the game draws its last frame of the old shot over the new one, the HUD included, until it is
// gone. func_80095150 copies that frame with the CPU, out of the framebuffer in RAM into a buffer of its own (304x240,
// 16-bit at 0x802EA000), and func_8009232C, called by the transition drawer func_800957F0, draws the copy as 38 texture
// rectangles of 6 rows each from x 8 to 312: whole for type 9, and for type 8 as two halves each that slide apart from
// the middle, doors opening on the new shot. The copy is what the CPU read from RAM, and RT64 renders that at the game's
// own 320x240 for it, so it holds the 4:3 frame and none of the wide picture's sides. Each strip spans the screen's
// scissor, and RT64 stretches a rectangle that spans it across the wide picture, so the old shot was drawn stretched
// sideways, jumping wider as the transition began.
//
// us.toml hooks the drawer's one call of func_8009232C on both sides: mg_card_strips_begin adds RT64's
// gEXSetRectAspect(G_EX_ASPECT_ADJUST) to the frame's display list, so its rectangles keep their 4:3 shape in the
// middle of the picture, and mg_card_strips_end puts G_EX_ASPECT_AUTO back. The old shot goes in the middle while the
// new one fills the sides. The drawer's other transitions need nothing: its tiles (type 10) are each narrower than
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
