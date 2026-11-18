// The Game Boy Color's golfers (Kid, Sherry, Azalea and Joe) at character select, with the General tab's Game Boy
// Golfers option (src/game/config.cpp).
//
// On a console they come only through a Transfer Pak holding Mario Golf for the Game Boy Color: overlay 15's link reads
// the cart's save into RAM for the session (it never writes them into the N64 save), and character select
// (func_8005CA48) offers each golfer it holds. The recompilation has no Transfer Pak (src/game/libultra.cpp), but the
// game has the four of its own, with the names and stats in its data, behind debug flag 0x54: with it on, character
// select puts them in its grid (func_8005CA48) in the layout that has room for them (func_8005C674), and nothing else
// reads the flag.
//
// func_800299F4 is the game's test of a debug flag, a bit of the 256 in D_801052F8, as it is, and answers flag 0x54 as
// on while the option is On; Original leaves the flags as the game has them.

#include "patches.h"

extern u8 D_801052F8[];

s32 mg_gb_golfers(void);

RECOMP_PATCH s32 func_800299F4(u32 flag) {
    if (flag >= 0x100) {
        return 0;
    }
    if (flag == 0x54 && mg_gb_golfers()) {
        return 1;
    }
    return (D_801052F8[flag >> 3] & (0x80 >> (flag & 7))) != 0;
}
