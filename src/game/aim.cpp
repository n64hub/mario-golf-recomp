// The marks the game draws as a shot is aimed where their points are on the screen, across to a quarter of a pixel,
// not the whole pixel the game truncates them to: the red pin flag that floats in the sky over the pin, and the label
// over the shot's landing point (its distance and the like).
//
// func_ovl0_8021C6B4, overlay 0's, draws both while a shot is aimed. It works out where the landing point is on the
// screen with func_80065650 and draws the label's texture rectangles around that pixel, then where the pin is, and
// draws the flag over it: an 8x16 sprite whose texture holds its four frames side by side, from 3 pixels left of the
// pin's x to 5 right of it, its foot no higher than 40 pixels down, bobbing a pixel or two. func_80065650 truncates x and
// y to whole pixels. Every console pixel is several of a modern window's, and RT64 (RT64_INTERPOLATE_RECT_SCROLLS,
// CMakeLists.txt) slides each rectangle between where each game frame put it, so as the view turned with the D-pad the
// flag went in steps of whole console pixels a game frame, one, then two, then three or four as the turn sped up, its
// pace changing 30 times a second against the course turning smoothly behind it (issue 166).
//
// us.toml hooks the function just after each of its two calls of func_80065650: mg_aim_begin notes where the display
// list's head stands and what func_80065650 dropped of the point's x (src/game/ball.cpp notes it as it truncates), and
// where that drawing ends (where its branches meet again) mg_aim_end moves each texture rectangle added since right by
// that fraction, to the quarter pixel. It keeps the game's size, so RT64 still pairs it with the last frame's and
// slides it between the two. RT64 itself puts a texture rectangle's edges on whole pixels, which slid from the quarter
// pixel one game frame had to the whole one the next has, so each began off where the last ended and the flag hopped
// as much as before: with RT64_SUBPIXEL_TEXRECTS (CMakeLists.txt) it draws one at the quarter pixel it is given. Only
// x is moved, as a turn of the view moves the marks across; y stays on the game's whole pixel. Whatever the window's
// shape.
//
// In a window wider than 4:3 (RT64's Aspect Ratio Expand) the marks also go on into the wide picture's sides with the
// course. The game draws a mark wherever its point is, but the RDP's scissor, the 3D's (8 to 312), cut it there, and
// RT64 draws a texture rectangle in the 4:3 frame, so the flag went out at a vertical line 8 pixels inside the 4:3
// frame's side as the view turned it toward the window's edge, with a quarter of the window still to go (issue 181).
// mg_aim_end hands each mark's rectangles to src/game/ball.cpp, which draws them as the ball's sprites are drawn into
// the sides (mg_widen_rects, its header says how), one the game cut at a side moved across by the same fraction there,
// as it is never moved here. It draws them that way wherever they are in a wide window, not only near a side: RT64
// slides a rectangle between two game frames only from one drawn the same way, and the flag drawn the one way and then
// the other slid tens of pixels the wrong way for a game frame as it came in past the 4:3 frame's side. RT64 pairs a
// rectangle with the last game frame's only if it moved less than its longer side, and the flag moves 22 to 26 pixels
// a game frame as the view turns fast: RT64 (N64Recomp-browser's rt64.patch) pairs it up to four times that far, as no
// other rectangle looks like it, or it held still between game frames and stepped against the course.
//
// The flag's texture is a tile of 32 by 16 texels, its four frames side by side, and the game draws the frame its
// counter (D_ovl0_80224B0C, 0 to 15, a quarter of it the frame) is on. Where the flag's left lies past the screen's,
// the game starts it at 0 and its texture as far into that frame as it cut (and one wholly past the left at 0, 0
// wide). ball.cpp puts such a rectangle back to its tile's start, which for the flag is the first frame's: the flag
// was drawn with the frames before its own to its left, and wholly past the side all four in a row, a line of flags
// across the wide picture's left that shrank into one as it came back in, and as its width changed each game frame
// RT64 paired it with nothing and it stepped (issue 190). mg_aim_end hands ball.cpp the frame's 8 texels for the
// flag, which it puts the rectangle back to instead.
//
// The game draws the flag (and the label) with the bilinear filter on, which on a console at the whole pixel it puts
// the flag on samples each texel at its own place, so the flag comes out crisp. RT64 samples it at every pixel of a
// larger picture, between texels, and blended in what lies beside them in the tile: the transparent texels' colour,
// pure blue, a column down the pole's left and bits under the cloth, and right of the cup the next frame's first
// column, its cup's lip and black base, a dark line on three of the four frames (issue 190). mg_aim_end turns the
// flag's own command that sets the palette's type (G_SETOTHERMODE_H, G_MDSFT_TEXTLUT, just before its rectangle) into
// one that sets the filter to point too, which samples each texel as the console does; the game sets bilinear again
// before anything else is drawn.

#include <cmath>
#include <cstdint>

float mg_last_projected_dx();
void mg_widen_rects(uint8_t* rdram, uint32_t start, uint32_t end, int cut_dx, int sprite_uls, int sprite_texels);

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;
// G_SETOTHERMODE_H with its shift and length: the palette's type alone (G_MDSFT_TEXTLUT, 14, 2 bits), and the filter
// and the palette's type together (G_MDSFT_TEXTFILT, 12, 4 bits), the filter's value 0 point.
constexpr uint32_t SET_TEXTLUT = 0xE3001001, SET_TEXTFILT_TEXTLUT = 0xE3001003;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The screen, in quarter pixels.
constexpr int SCREEN_WIDTH = 320 * 4;
// The commands one mark adds, at most (the label's letters and the flag, a few hundred).
constexpr uint32_t MAX_COMMANDS = 4096;
// The flag's animation counter (overlay 0's data), 0 to 15, a quarter of it the frame the game draws, and the width of
// a frame in the flag's tile.
constexpr uint32_t FLAG_COUNTER = 0x80224B0C;
constexpr int FLAG_FRAME_TEXELS = 8;

uint8_t* g_rdram;
bool g_open;
uint32_t g_start;
int g_dx; // quarter pixels

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

} // namespace

// Just after func_80065650 works out a mark's point: what it dropped of x, and where the mark's commands start.
extern "C" void mg_aim_begin(uint8_t* rdram) {
    g_rdram = rdram;
    g_start = rd_w(MG_GFX_HEAD);
    g_dx = int(std::lround(mg_last_projected_dx() * 4.0f));
    g_open = true;
}

// Where the mark's drawing ends: each texture rectangle it added moved across by the fraction. flag: 1 for the flag's
// drawing (its frame's texels go to mg_widen_rects), 0 for the label's.
extern "C" void mg_aim_end(uint8_t* rdram, int flag) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    uint32_t end = rd_w(MG_GFX_HEAD);
    if (end <= g_start || (end - g_start) / 8 > MAX_COMMANDS || (g_start & 7) != 0) {
        return;
    }
    for (uint32_t addr = g_start; g_dx != 0 && addr + 24 <= end; addr += 8) {
        uint32_t w0 = rd_w(addr);
        if ((w0 >> 24) != G_TEXRECT || (rd_w(addr + 8) >> 24) != G_RDPHALF_1 || (rd_w(addr + 16) >> 24) != G_RDPHALF_2) {
            continue;
        }
        uint32_t w1 = rd_w(addr + 4);
        int ulx = int((w1 >> 12) & 0xFFF), lrx = int((w0 >> 12) & 0xFFF);
        if (ulx > 0 && lrx > ulx && lrx < SCREEN_WIDTH && ulx + g_dx > 0 && lrx + g_dx < SCREEN_WIDTH) {
            wr_w(addr, (w0 & 0xFF000FFF) | (uint32_t(lrx + g_dx) << 12));
            wr_w(addr + 4, (w1 & 0xFF000FFF) | (uint32_t(ulx + g_dx) << 12));
        }
        addr += 16;
    }
    // The flag sampled as on a console (above): its last palette type command before its rectangle sets point too.
    if (flag) {
        uint32_t lut = 0;
        for (uint32_t addr = g_start; addr + 8 <= end; addr += 8) {
            uint32_t w0 = rd_w(addr);
            if (w0 == SET_TEXTLUT) {
                lut = addr;
            }
            else if ((w0 >> 24) == G_TEXRECT && lut != 0) {
                wr_w(lut, SET_TEXTFILT_TEXTLUT);
                wr_w(lut + 4, rd_w(lut + 4) & 0xC000);
                lut = 0;
            }
        }
    }
    int frame = flag ? int(rd_w(FLAG_COUNTER) & 0xF) / 4 : 0;
    mg_widen_rects(rdram, g_start, end, g_dx, frame * FLAG_FRAME_TEXELS, flag ? FLAG_FRAME_TEXELS : 0);
}
