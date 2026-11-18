// The staff credits' last picture at the picture's full height in a wide window, its shape kept, the rest black, as
// screen.cpp draws the title and the menus (RT64's Aspect Ratio Expand).
//
// The credits (scene 0x1E, overlay 16) end on a 304x224 picture that fades in over them and stays until START or A:
// func_ovl16_801F7554, the credits' drawing, draws it as 38 texture rectangles of 6 rows each from x 8 to 312, under
// the game's border scissor of 8, 8 to 312, 232, which it sets at its start with bilinear filtering, in one cycle,
// TEXEL0's colour at the primitive colour's alpha while it fades in and opaque once it is in. RT64 stretches a
// rectangle as wide as its scissor across the window when the frame draws 3D, and the credits draw the course in a box
// beside the names, so the picture filled the wide window, and snapped back to 4:3 when START began the transition out
// (whose strips card.cpp draws at 4:3 here). The title's painting is drawn the same way over no 3D, which RT64 keeps at
// 4:3 by itself.
//
// So us.toml hooks the drawing where the picture's strips begin (0x801F7D9C, reached whenever the picture is drawn,
// faded or opaque, its render mode, combine and alpha set) and where the function puts its list's head back
// (0x801F8000). The function keeps the list's head in its stack frame (sp + 0x10) while it draws, so that is where
// these add commands. At the start:
// - a black rectangle at the picture's alpha, from the screen's left to its right under the border scissor, which RT64
//   stretches across the window as it did the picture: the course's box reaches past the 4:3 frame, where the
//   stretched picture covered it and the picture at its shape no longer does, so the sides go black as the picture
//   fades in, as everything under it does on a console. It is under the picture in the middle too, which darkens what
//   the picture fades in over a little faster than the console does, for the second or two the fade lasts. Then the
//   function's own combine again;
// - the border scissor and the filter again, and at the end everything from there to the head goes through screen.cpp
//   (mg_screen_fill_range), which scales the scissor and the strips to the picture's full height and keeps the strips
//   at their shape (gEXSetRectAspect(G_EX_ASPECT_ADJUST)), point sampled; then the scissor and the filter are set back
//   as the function set them.
// With Aspect Ratio Original, or a window no wider than 4:3, nothing is added.

#include <cstdint>

extern "C" void mg_screen_fill_range(uint8_t* rdram, uint32_t list, uint32_t end);
extern "C" bool mg_screen_fills();

namespace {

// Where func_ovl16_801F7554 keeps the list's head while it draws: sp + 0x10.
constexpr uint32_t HEAD_ON_STACK = 0x10;
// The game's border scissor, 8, 8 to 312, 232, and the bilinear filter (G_SETOTHERMODE_H, G_MDSFT_TEXTFILT), both as
// the function sets them at its start.
constexpr uint32_t BORDER_SCISSOR_W0 = 0xED020020, BORDER_SCISSOR_W1 = 0x004E03A0;
constexpr uint32_t SET_TEXTURE_FILTER = 0xE3001201, G_TF_BILERP = 0x2000;
// F3DEX2's commands this writes and reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_SETCOMBINE = 0xFC, G_FILLRECT = 0xF6, G_RDPPIPESYNC_W0 = 0xE7000000;
// The combine for black at the primitive colour's alpha, in both cycles: (0 - 0) * 0 + 0, and (0 - 0) * 0 + PRIMITIVE.
constexpr uint32_t BLACK_COMBINE_W0 = 0xFCFFFFFF, BLACK_COMBINE_W1 = 0xFFFFF7DF;
// The screen, 0, 0 to 320, 240, in a G_FILLRECT's quarter pixels: lower right in the first word, upper left in the
// second.
constexpr uint32_t SCREEN_RECT_W0 = (G_FILLRECT << 24) | ((320 * 4) << 12) | (240 * 4), SCREEN_RECT_W1 = 0;
// How far back from the head the function's combine is looked for: it is the command just before.
constexpr uint32_t COMBINE_LOOKBACK = 4;

uint32_t g_from = 0;

uint32_t rd_w(uint8_t* rdram, uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF));
}

void wr_w(uint8_t* rdram, uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF)) = value;
}

// Adds a command at the head the function keeps at sp + 0x10.
void put(uint8_t* rdram, uint32_t sp, uint32_t w0, uint32_t w1) {
    uint32_t head = rd_w(rdram, sp + HEAD_ON_STACK);
    wr_w(rdram, head, w0);
    wr_w(rdram, head + 4, w1);
    wr_w(rdram, sp + HEAD_ON_STACK, head + 8);
}

// The black rectangle over the window under the picture, at its alpha, and the function's combine after it. Nothing if
// the function's combine isn't just before the head.
void put_black(uint8_t* rdram, uint32_t sp) {
    uint32_t head = rd_w(rdram, sp + HEAD_ON_STACK);
    uint32_t combine = 0;
    for (uint32_t i = 1; i <= COMBINE_LOOKBACK && combine == 0; i++) {
        if ((rd_w(rdram, head - 8 * i) >> 24) == G_SETCOMBINE) {
            combine = head - 8 * i;
        }
    }
    if (combine == 0) {
        return;
    }
    uint32_t combine_w0 = rd_w(rdram, combine), combine_w1 = rd_w(rdram, combine + 4);
    put(rdram, sp, BORDER_SCISSOR_W0, BORDER_SCISSOR_W1);
    put(rdram, sp, BLACK_COMBINE_W0, BLACK_COMBINE_W1);
    put(rdram, sp, SCREEN_RECT_W0, SCREEN_RECT_W1);
    put(rdram, sp, G_RDPPIPESYNC_W0, 0);
    put(rdram, sp, combine_w0, combine_w1);
}

// Adds the border scissor and the bilinear filter at the head, and returns where they begin.
uint32_t put_border(uint8_t* rdram, uint32_t sp) {
    uint32_t head = rd_w(rdram, sp + HEAD_ON_STACK);
    put(rdram, sp, BORDER_SCISSOR_W0, BORDER_SCISSOR_W1);
    put(rdram, sp, SET_TEXTURE_FILTER, G_TF_BILERP);
    return head;
}

} // namespace

extern "C" void mg_credits_picture_begin(uint8_t* rdram, uint32_t sp) {
    g_from = 0;
    if (!mg_screen_fills()) {
        return;
    }
    put_black(rdram, sp);
    g_from = put_border(rdram, sp);
}

extern "C" void mg_credits_picture_end(uint8_t* rdram, uint32_t sp) {
    if (g_from == 0) {
        return;
    }
    mg_screen_fill_range(rdram, g_from, rd_w(rdram, sp + HEAD_ON_STACK));
    g_from = 0;
    put_border(rdram, sp);
}
