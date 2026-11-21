// The game's sound effects (WIP: a log of each one the game starts, to find the courses' animal calls).
//
// Every sound the game plays starts in one of two functions of its sound layer, func_80050BF0 and func_80050DA0
// (us.toml's hooks at their first instruction), each given a sound ID (below 0x215), a channel slot (of 32) and a
// volume, which start it in libmus through func_8005F0E0. With MG_SOUND_LOG set, each start is printed with its
// caller.

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "recomp.h"

namespace {
    const bool sound_log = std::getenv("MG_SOUND_LOG") != nullptr;
    const auto start_time = std::chrono::steady_clock::now();
}

// At the first instruction of func_80050BF0 (which = 0) or func_80050DA0 (which = 1): 1 to start nothing.
extern "C" int mg_sound_start(uint8_t* rdram, recomp_context* ctx, int which) {
    (void)rdram;
    if (sound_log) {
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
        std::fprintf(stderr, "mg_sound %.2f f%d id=0x%03X slot=%d vol=%d ra=0x%08X\n", t, which,
            (unsigned)ctx->r4 & 0xFFFF, (int)(int32_t)ctx->r5, (int)(int32_t)ctx->r6, (unsigned)ctx->r31);
    }
    return 0;
}
