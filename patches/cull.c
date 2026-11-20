// The course's culling at the wide picture's width (RT64's Aspect Ratio Expand): terrain at the picture's sides drawn
// rather than left out.
//
// The course's drawing (func_80062A8C) asks func_800653CC of each block of the course's terrain whether a sphere round
// it can be seen: it puts the sphere's centre in the camera's view (the view matrix and the projection's scales that
// func_80065070 keeps for it, D_800E16F0 and D_800E1730 on), leaves out what lies behind the camera, and then what lies
// past the 4:3 screen's sides (D_800E1738, half the viewport's width) or its top and bottom. RT64 widens the picture
// but not the game's test, so in a window wider than 4:3 the terrain past the 4:3 frame's sides came and went with the
// camera, and a hole's flyover showed water where the course goes on.
//
// This is the game's function as it is, with the sides' half-width widened by mg_cull_width_scale (src/game/cull.cpp):
// the window's aspect ratio over 4:3 with Aspect Ratio Expand, 1 with Original, so Original culls as the console does.
// Its return, as the game's: 0 behind the camera, 1 past a side, 2 past the top or bottom, else the depth (negative).

#include "patches.h"

extern f32 D_800E16EC;
extern f32 D_800E16F0[4][4];
extern f32 D_800E1730;
extern f32 D_800E1734;
extern s32 D_800E1738;
extern s32 D_800E173C;

f32 mg_cull_width_scale(void);

RECOMP_PATCH s32 func_800653CC(f32 x, f32 y, f32 z, f32 radius) {
    f32 depth = D_800E16F0[0][2] * x + D_800E16F0[1][2] * y + D_800E16F0[2][2] * z + D_800E16F0[3][2] - radius;
    f32 inverse;
    f32 half_width;
    f32 half_height;
    f32 view;

    if ((s32)depth >= 0) {
        return 0;
    }

    inverse = ((s32)D_800E16EC == 1) ? 1.0f : 1.0f / depth;

    half_width = (f32)D_800E1738 * mg_cull_width_scale();
    view = D_800E16F0[0][0] * x + D_800E16F0[1][0] * y + D_800E16F0[2][0] * z + D_800E16F0[3][0];
    if (0.0f < view) {
        if (half_width < D_800E1730 * (view - radius) * inverse) {
            return 1;
        }
    } else if (D_800E1730 * (view + radius) * inverse < -half_width) {
        return 1;
    }

    half_height = (f32)D_800E173C;
    view = D_800E16F0[0][1] * x + D_800E16F0[1][1] * y + D_800E16F0[2][1] * z + D_800E16F0[3][1];
    if (0.0f < view) {
        if (D_800E1734 * (view - radius) * inverse < -half_height) {
            return 2;
        }
    } else if (half_height < D_800E1734 * (view + radius) * inverse) {
        return 2;
    }

    return (s32)depth;
}
