// How much wider than the game's 4:3 the picture is: the window's aspect ratio over 4:3 with the Graphics tab's Aspect
// Ratio Expand, where RT64 widens the picture to the window, and 1 with Original, or in a window no wider than 4:3,
// where the game's own 4:3 fills the picture. The course's culling reads it (patches/cull.c), and so does the sun's
// light on the sky (src/game/backdrop.cpp), and the title's and menus' scale (src/game/screen.cpp). The culling's top
// and bottom read how much taller than the game's 224 lines inside its border the picture is: 240 over 224 with
// Expand, where src/game/border.cpp draws the course through the border, in any window, and 1 with Original.
#include <cstdint>

#include "recomp.h"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

float mg_wide_scale() {
    float scale = 1.0f;
    if (ultramodern::renderer::get_graphics_config().ar_option == ultramodern::renderer::AspectRatio::Expand) {
        int width = 0;
        int height = 0;
        recompui::get_window_size(width, height);
        if (width > 0 && height > 0) {
            float wide = (float(width) / float(height)) / (4.0f / 3.0f);
            if (wide > scale) {
                scale = wide;
            }
        }
    }
    return scale;
}

// Whether RT64 widens the picture to the window (Aspect Ratio Expand), however wide the window is.
bool mg_aspect_expand() {
    return ultramodern::renderer::get_graphics_config().ar_option == ultramodern::renderer::AspectRatio::Expand;
}

extern "C" void mg_cull_width_scale(uint8_t*, recomp_context* ctx) {
    ctx->f0.fl = mg_wide_scale();
}

extern "C" void mg_cull_height_scale(uint8_t*, recomp_context* ctx) {
    ctx->f0.fl = mg_aspect_expand() ? 240.0f / 224.0f : 1.0f;
}
