// The old shot of a transition, drawn over the wide picture's whole width (RT64's Aspect Ratio Expand).
//
// As the intro cuts between its shots (transition type 9) or crossfades between them (type 1: from Luigi's swing to the
// overhead shot of the putting green), and as a shot's camera cuts to a view from the green while the ball flies or a
// menu opens the next (type 8), the game draws its last frame of the old shot over the new one, the HUD included, until
// it is gone. func_80095150 copies that frame with the CPU, out of the framebuffer in RAM into a buffer of its own
// (304x224, 16-bit, packed, in the z-buffer at 0x802EA000), and func_8009232C, called by the transition drawer
// func_800957F0, draws the copy as 38 texture rectangles of 6 rows each from x 8 to 312: whole and fading for types 1
// and 9 (the same strips, type 1's fade half as fast), and for type 8 as two halves each that slide apart from the
// middle, doors opening on the new shot.
// The copy is what the CPU read from RAM, and RT64 renders that at the game's own 320x240 for it, so it holds the 4:3
// frame and none of the wide picture's sides: drawn as the game draws it, it was stretched sideways, jumping wider as
// the transition began, and kept at 4:3 it was a box in the middle with the new shot and its HUD at its sides.
//
// So here the copy is the RDP's, of the framebuffer RT64 has: us.toml hooks the drawer before its call of
// func_80095150 (mg_card_check: whether that call copies, at 0x800C72F8, so a transition starts this frame) and on both
// sides of its call of func_8009232C. On the frame a transition starts, mg_card_strips_begin calls a display list of
// its own that copies the frame before (the last framebuffer, which the drawer keeps at 0x800FED10) into a 320x240
// image of its own, in RT64 a framebuffer drawn at the wide picture's size. Both are in the runtime's heap past the
// game's RAM, which RT64 reads with gEXSetRDRAMExtended: an image at the game's own copy, in its z-buffer, is one RT64
// has had as depth, and the strips read that depth as colours. mg_card_strips_end then rewrites the strips the game
// added to read that image (a load of 6 of its rows each, in place of a 304-wide block), and widens those that start or
// end at the picture's border (x 8 or 312) to its edge, so the old shot fills the wide picture. RT64 draws a rectangle
// that reads a framebuffer stretched across the picture, as the framebuffer it reads was drawn, so the doors slide
// over the whole width, and in a 4:3 menu the old one's black sides come with it.
//
// screen.cpp's wipe between the title and the main menu makes its copy of the frame before the same way, into an image
// of its own (mg_copy_frame_list).
//
// Any other strips the drawer draws (another mode or type, a copy elsewhere) keep their 4:3 shape in the middle of the
// picture (gEXSetRectAspect(G_EX_ASPECT_ADJUST) around them). The drawer's other transitions need nothing: its tiles
// (type 10) are each narrower than the screen, and the card that turns over to a course's next shot (type 2) is a
// model, both kept at 4:3 by RT64.

#include <cstdint>

#include "recomp.h"
#include "librecomp/addresses.hpp"

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

namespace {

// The word holding the frame's display list's head, where the game adds its next command.
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The framebuffer the frame draws into (nuGfx's), and the one the frame before drew into (the drawer's).
constexpr uint32_t MG_CFB = 0x801B69E8;
constexpr uint32_t MG_LAST_CFB = 0x800FED10;
// The transition's copy (its address), its type and its mode, and func_80095150's flag to make the copy.
constexpr uint32_t MG_COPY = 0x800C5EE0;
constexpr uint32_t MG_TYPE = 0x800C5EE8;
constexpr uint32_t MG_MODE = 0x800C7304;
constexpr uint32_t MG_COPY_FLAG = 0x800C72F8;

// Where the game keeps its copy: its z-buffer, 320x240 at 16 bits.
constexpr uint32_t COPY_ADDR = 0x802EA000;
// A strip's 6 rows in the game's copy, 304 pixels at 16 bits each.
constexpr uint32_t STRIP_BYTES = 304 * 2 * 6;
constexpr uint32_t STRIPS = 38;

uint32_t rd_w(uint8_t* rdram, uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF));
}

void wr_w(uint8_t* rdram, uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(rdram + (addr & 0x7FFFFF)) = value;
}

// Adds a command at the list's head, as the game adds its own.
void put(uint8_t* rdram, uint32_t w0, uint32_t w1) {
    uint32_t head = rd_w(rdram, MG_GFX_HEAD);
    wr_w(rdram, head, w0);
    wr_w(rdram, head + 4, w1);
    wr_w(rdram, MG_GFX_HEAD, head + 8);
}

void put(uint8_t* rdram, const GfxCommand& c) {
    put(rdram, c.values.word0, c.values.word1);
}

// The display list that copies the frame before, and the 320x240 image it copies it into, in the runtime's heap past
// the game's 8 MiB (RT64 reads them with gEXSetRDRAMExtended).
constexpr uint32_t COPY_LIST_CMDS = 16 + 40 * 10;
constexpr uint32_t IMAGE_BYTES = 320 * 240 * 2;
uint32_t* copy_list = nullptr;
uint32_t copy_list_addr = 0;
uint32_t image_addr = 0;

// Whether the transition the drawer draws is one whose copy is made here, whether this frame starts one, and whether
// its copy was made.
bool wide_copy = false;
bool starting = false;
bool copied = false;
// Where the strips start in the frame's list, when they are to read that copy.
uint32_t strips_from = 0;

bool wide_transition(uint8_t* rdram) {
    const uint32_t type = rd_w(rdram, MG_TYPE);
    return (rd_w(rdram, MG_MODE) == 0) && ((type == 1) || (type == 8) || (type == 9)) &&
        (rd_w(rdram, MG_COPY) == COPY_ADDR);
}

// Writes a copy's display list at list: the 240 rows of the frame before, 6 at a time (TMEM's 4 KiB), copied (the RDP's
// copy mode) into image, then the frame's own framebuffer and state back.
void write_copy_list(uint32_t* list, uint32_t image, uint32_t from, uint32_t to) {
    uint32_t* c = list;
    auto cmd = [&c](uint32_t w0, uint32_t w1) { *c++ = w0; *c++ = w1; };
    auto ex = [&cmd](const GfxCommand& g) { cmd(g.values.word0, g.values.word1); };
    GfxCommand g[1];
    cmd(0xE7000000, 0);                         // G_RDPPIPESYNC
    gEXPushOtherMode(g); ex(g[0]);
    gEXPushScissor(g); ex(g[0]);
    cmd(0xFF10013F, image);                     // G_SETCIMG: RGBA16, 320 wide
    cmd(0xED000000, (320 << 14) | (240 << 2));  // G_SETSCISSOR (0, 0)-(320, 240)
    cmd(0xE3000A01, 0x00200000);                // G_SETOTHERMODE_H: G_CYC_COPY
    cmd(0xE3001001, 0x00000000);                // G_SETOTHERMODE_H: G_TT_NONE
    cmd(0xE200001C, 0x00000000);                // G_SETOTHERMODE_L: no alpha compare, no blending
    for (uint32_t row = 0; row < 240; row += 6) {
        const uint32_t ul = row << 2, lr = (row + 5) << 2;
        cmd(0xFD10013F, from);                  // G_SETTIMG: RGBA16, 320 wide
        cmd(0xF510A000, 0x07000000);            // G_SETTILE 7 (the load's): 80 words a row
        cmd(0xE6000000, 0);                     // G_RDPLOADSYNC
        cmd(0xF4000000 | ul, 0x07000000 | (319 << 14) | lr); // G_LOADTILE (0, row)-(319, row + 5)
        cmd(0xE7000000, 0);
        cmd(0xF510A000, 0x00000000);            // G_SETTILE 0
        cmd(0xF2000000 | ul, (319 << 14) | lr); // G_SETTILESIZE (0, row)-(319, row + 5)
        // G_TEXRECT (0, row)-(319, row + 5): copy mode's corners are both drawn
        cmd(0xE4000000 | (319 << 14) | lr, (0 << 24) | ul);
        cmd(0xE1000000, row << 5);              // s 0, t row
        cmd(0xF1000000, 0x10000400);            // dsdx 4 (copy mode's 4 pixels a cycle), dtdy 1
    }
    cmd(0xE7000000, 0);
    cmd(0xFF10013F, to);                        // the frame's own framebuffer back
    gEXPopScissor(g); ex(g[0]);
    gEXPopOtherMode(g); ex(g[0]);
    cmd(0xDF000000, 0);                         // G_ENDDL
}

} // namespace

// A copy of the frame before for screen.cpp's tile wipe, in a block of its own: writes its display list, which copies
// the framebuffer at from into a 320x240 image and sets the framebuffer at to back, and returns the list's address
// (to be called with gEXSetRDRAMExtended on), the image's in *image.
extern "C" uint32_t mg_copy_frame_list(uint8_t* rdram, uint32_t from, uint32_t to, uint32_t* image) {
    static uint32_t* list = nullptr;
    static uint32_t list_addr = 0, image_at = 0;
    if (list == nullptr) {
        uint8_t* block = static_cast<uint8_t*>(recomp::alloc(rdram, COPY_LIST_CMDS * 8 + IMAGE_BYTES + 64));
        list = reinterpret_cast<uint32_t*>(block);
        list_addr = 0x80000000U + uint32_t(block - rdram);
        image_at = (list_addr + COPY_LIST_CMDS * 8 + 63) & ~63U;
    }
    write_copy_list(list, image_at, from, to);
    *image = image_at;
    return list_addr;
}

extern "C" void mg_card_check(uint8_t* rdram) {
    starting = (rd_w(rdram, MG_COPY_FLAG) != 0);
    if (starting) {
        wide_copy = wide_transition(rdram);
        copied = false;
    }
}

extern "C" void mg_card_strips_begin(uint8_t* rdram) {
    GfxCommand c[1];
    gEXEnable(c);
    put(rdram, c[0]);
    const bool wide = wide_copy && wide_transition(rdram) && (copied || starting);
    if (wide && !copied) {
        if (copy_list == nullptr) {
            uint8_t* block = static_cast<uint8_t*>(recomp::alloc(rdram, COPY_LIST_CMDS * 8 + IMAGE_BYTES + 64));
            copy_list = reinterpret_cast<uint32_t*>(block);
            copy_list_addr = 0x80000000U + uint32_t(block - rdram);
            image_addr = (copy_list_addr + COPY_LIST_CMDS * 8 + 63) & ~63U;
        }
        write_copy_list(copy_list, image_addr, rd_w(rdram, MG_LAST_CFB), rd_w(rdram, MG_CFB));
        gEXSetRDRAMExtended(c, 1);
        put(rdram, c[0]);
        put(rdram, 0xDE000000, copy_list_addr); // G_DL
        gEXSetRDRAMExtended(c, 0);
        put(rdram, c[0]);
        copied = true;
    }
    starting = false;
    gEXSetRectAspect(c, wide ? G_EX_ASPECT_AUTO : G_EX_ASPECT_ADJUST);
    put(rdram, c[0]);
    strips_from = 0;
    if (wide) {
        // The strips read the image past the game's RAM.
        gEXSetRDRAMExtended(c, 1);
        put(rdram, c[0]);
        strips_from = rd_w(rdram, MG_GFX_HEAD);
    }
}

// Rewrites the strips func_8009232C added since mg_card_strips_begin to read the image made there. A strip is
//   G_SETTIMG (RGBA16, 1 wide) at the game's copy plus 0xE40 bytes a strip, G_SETTILE 7 (0 words a row), G_LOADBLOCK
//   of its 1824 texels (the last strip's 608), G_SETTILE 0 (76 words a row) and G_SETTILESIZE (0, 0)-(303, 5) (the
//   first strip only), and one or two G_TEXRECTs (each with its G_RDPHALF_1 and _2: s and t, dsdx and dtdy) of rows
//   8 + 6k to 14 + 6k,
// and becomes G_SETTIMG (RGBA16, 320 wide) at the image, G_LOADTILE of its 6 rows (80 words a row), G_SETTILESIZE
// (0, 0)-(319, 5), and the rectangles, each of which starts at the image's column 8 more (the copy left out the 8
// border columns), or at the screen's left edge (x 0) if it started at the border (x 8), and ends there (x 320) if it
// ended at the right border (x 312). A list that isn't that shape is drawn as the game made it, at 4:3.
extern "C" void mg_card_strips_end(uint8_t* rdram) {
    GfxCommand c[1];
    if (strips_from != 0) {
        const uint32_t to = rd_w(rdram, MG_GFX_HEAD);
        uint32_t row = 0;
        bool shaped = true;
        for (uint32_t a = strips_from; a < to; a += 8) {
            const uint32_t w0 = rd_w(rdram, a), w1 = rd_w(rdram, a + 4);
            switch (w0 >> 24) {
            case 0xFD: {
                const uint32_t k = (w1 - COPY_ADDR) / STRIP_BYTES;
                shaped = shaped && (w0 == 0xFD100000) && (w1 >= COPY_ADDR) && (k < STRIPS) &&
                    ((w1 - COPY_ADDR) % STRIP_BYTES == 0);
                row = 8 + 6 * k;
                break;
            }
            case 0xF5:
                shaped = shaped && ((w0 == 0xF5100000) || (w0 == 0xF5109800));
                break;
            case 0xF3:
                // 6 rows of 304 texels (the last strip's 2), 19 words a row.
                shaped = shaped && (w0 == 0xF3000000) && ((w1 & 0xFF000FFF) == 0x0700001B) &&
                    ((((w1 >> 12) & 0xFFF) + 1) % 304 == 0);
                break;
            case 0xF2:
                shaped = shaped && (w0 == 0xF2000000) && (w1 == 0x004BC014);
                break;
            case 0xE4: {
                const uint32_t lrx = (w0 >> 12) & 0xFFF, ulx = (w1 >> 12) & 0xFFF;
                const uint32_t s = rd_w(rdram, a + 12) >> 16;
                shaped = shaped && ((rd_w(rdram, a + 8) >> 24) == 0xE1) && (ulx >= (8 << 2)) && (lrx <= (312 << 2)) &&
                    (s + (8 << 5) <= 0xFFFF);
                break;
            }
            default:
                break;
            }
        }
        if (shaped) {
            for (uint32_t a = strips_from; a < to; a += 8) {
                const uint32_t w0 = rd_w(rdram, a), w1 = rd_w(rdram, a + 4);
                switch (w0 >> 24) {
                case 0xFD:
                    row = 8 + 6 * ((w1 - COPY_ADDR) / STRIP_BYTES);
                    wr_w(rdram, a, 0xFD10013F);
                    wr_w(rdram, a + 4, image_addr);
                    break;
                case 0xF5:
                    wr_w(rdram, a, 0xF510A000);
                    break;
                case 0xF3:
                    wr_w(rdram, a, 0xF4000000 | (row << 2));
                    wr_w(rdram, a + 4, 0x07000000 | (319 << 14) | ((row + 5) << 2));
                    break;
                case 0xF2:
                    wr_w(rdram, a + 4, (319 << 14) | (5 << 2));
                    break;
                case 0xE4: {
                    uint32_t lrx = (w0 >> 12) & 0xFFF, ulx = (w1 >> 12) & 0xFFF;
                    uint32_t st = rd_w(rdram, a + 12);
                    if (ulx == (8 << 2)) {
                        ulx = 0;
                    }
                    else {
                        st += (8 << 5) << 16;
                    }
                    if (lrx == (312 << 2)) {
                        lrx = 320 << 2;
                    }
                    wr_w(rdram, a, (w0 & 0xFF000FFF) | (lrx << 12));
                    wr_w(rdram, a + 4, (w1 & 0xFF000FFF) | (ulx << 12));
                    wr_w(rdram, a + 12, st);
                    break;
                }
                default:
                    break;
                }
            }
        }
        else {
            // Not the strips this knows: they are drawn as before a copy was made here, at 4:3.
            gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
            wr_w(rdram, strips_from - 16, c[0].values.word0);
            wr_w(rdram, strips_from - 12, c[0].values.word1);
        }
        gEXSetRDRAMExtended(c, 0);
        put(rdram, c[0]);
        strips_from = 0;
    }
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    put(rdram, c[0]);
}
