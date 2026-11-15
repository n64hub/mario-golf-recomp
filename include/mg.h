#ifndef __MG_H__
#define __MG_H__

#include <cstdint>
#include <string_view>

namespace mg {
    inline constexpr std::u8string_view program_id = u8"MarioGolfRecompiled";
    inline constexpr std::string_view program_name = "Mario Golf: Recompiled";

    // The game's code sections (register_overlays.cpp) and the patches over it (register_patches.cpp).
    void register_overlays();
    void register_patches();

    // The settings (config.cpp): RecompFrontend's tabs.
    void init_config();

    // The TLB's window (tlb.cpp): hands the runtime its osVirtualToPhysical and RSP-task callbacks, before the game runs.
    void init_tlb(uint8_t* rdram);
}

#endif
