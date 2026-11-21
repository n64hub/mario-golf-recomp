// The courses' ambience at the Sound tab's Ambient Volume (src/game/config.cpp).
//
// On a hole, overlay 0's func_ovl0_802078F4 runs every game frame, and every 240th one picks one of seven sound IDs at
// random from its course's row of a table (D_ovl0_80223EF0, seven words a row, -1 for none; the row is
// func_80051FCC's, the course but for Mini-Golf's), and plays it through the sound layer's func_80050DA0 on channel
// slot 10 at volume 0x5F, panned at random. Toad Highlands', Koopa Park's and Mini-Golf's rows are birds (0x16, 0x18,
// 0x19, one in seven none), Mario's Star's 0x1A, 0x1B, 0x2B and 0x18, Yoshi's Island's its jungle's seven (0x1E,
// 0xE3-0xE8, never none), and Shy Guy Desert's and Boo Valley's all none. No other sound comes that way: on Yoshi's
// Island's tee a minute with no input starts only these, one every 8.3 s. So us.toml's hook at that call scales only
// its volume: 100% (the default) plays it as a console does, 0% silences it. The game's random numbers are drawn as
// before, so nothing else changes. With MG_SOUND_LOG set, each call is printed with the volume it is played at.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "recompui/config.h"
#include "mg.h"

namespace {
    const bool sound_log = std::getenv("MG_SOUND_LOG") != nullptr;
}

// The ID func_ovl0_802078F4 passes func_80050DA0 (us.toml) and its volume, which is returned scaled by Ambient Volume.
extern "C" int32_t mg_ambient_volume(int32_t id, int32_t volume) {
    int32_t scaled = static_cast<int32_t>(std::lround(volume * mg::ambient_volume() / 100.0));
    if (sound_log) {
        std::fprintf(stderr, "mg_ambient id=0x%03X volume=%d played at %d\n", (unsigned)id, (int)volume, (int)scaled);
    }
    return scaled;
}
