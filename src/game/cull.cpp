// How much wider than the game's 4:3 the picture is, for the course's culling (patches/cull.c): the window's aspect
// ratio over 4:3 with the Graphics tab's Aspect Ratio Expand, where RT64 widens the picture to the window, and 1 with
// Original, or in a window no wider than 4:3, where the game's own test fits the picture.
#include <cstdint>

#include "recomp.h"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

extern "C" void mg_cull_width_scale(uint8_t* rdram, recomp_context* ctx) {
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
    ctx->f0.fl = scale;
}
