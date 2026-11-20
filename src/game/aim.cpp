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
// x is moved, as a turn of the view moves the marks across; y stays on the game's whole pixel. A rectangle the game
// cut at a side of the screen is left as it is. Whatever the window's shape.

#include <cmath>
#include <cstdint>

float mg_last_projected_dx();

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_TEXRECT = 0xE4, G_RDPHALF_1 = 0xE1, G_RDPHALF_2 = 0xF1;

// The display list's head, which the game's drawing functions advance (the decomp's data at 0x801B93DC).
constexpr uint32_t MG_GFX_HEAD = 0x801B93DC;
// The screen, in quarter pixels.
constexpr int SCREEN_WIDTH = 320 * 4;
// The commands one mark adds, at most (the label's letters and the flag, a few hundred).
constexpr uint32_t MAX_COMMANDS = 4096;

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

// Where the mark's drawing ends: each texture rectangle it added moved across by the fraction.
extern "C" void mg_aim_end(uint8_t* rdram) {
    if (!g_open) {
        return;
    }
    g_open = false;
    g_rdram = rdram;
    uint32_t end = rd_w(MG_GFX_HEAD);
    if (g_dx == 0 || end <= g_start || (end - g_start) / 8 > MAX_COMMANDS || (g_start & 7) != 0) {
        return;
    }
    for (uint32_t addr = g_start; addr + 24 <= end; addr += 8) {
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
}
