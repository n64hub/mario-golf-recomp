// libultra functions the game links that N64Recomp leaves out (its ignored_funcs) and the runtime doesn't implement,
// so each call to one in RecompiledFuncs/ comes here. The TLB's are src/game/tlb.cpp.
#include <cstdint>
#include <cstdio>

#include "recomp.h"

// The controller's accessory bus, which the game reads only for a Transfer Pak (libultra's osGbpak calls, for its link
// with Mario Golf on the Game Boy Color, whose golfers it brings in). None is plugged in here, as Mario Tennis's recomp
// has it: each answers PFS_ERR_NOPACK, so the game finds no Transfer Pak, as on a console with none. A Controller Pak
// and the Rumble Pak are the runtime's own (osPfs, osMotor), which don't come here.
static constexpr int32_t PFS_ERR_NOPACK = 1;

extern "C" void __osPfsGetStatus_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = PFS_ERR_NOPACK;
}

extern "C" void __osContRamRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = PFS_ERR_NOPACK;
}

extern "C" void __osContRamWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = PFS_ERR_NOPACK;
}

// osDpGetCounters(u32 counters[4]): the RDP's clock, command-busy, pipe-busy and TMEM counters, which NuSystem's
// scheduler (nuScExecuteGraphics) reads for its performance meter. There is no RDP to count.
extern "C" void osDpGetCounters_recomp(uint8_t* rdram, recomp_context* ctx) {
    for (int i = 0; i < 4; i++) {
        MEM_W(i * 4, ctx->r4) = 0;
    }
}

// osViGetCurrentMode(): the VI mode's type, OS_VI_NTSC_LAN1 (320x240, non-interlaced), the mode the game sets.
extern "C" void osViGetCurrentMode_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 2;
}

// The VI manager's own internals: the game's viMgrMain calls them, and the runtime runs the VI manager itself
// (osCreateViManager), so the game's never runs.
extern "C" void __osViGetCurrentContext_recomp(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "__osViGetCurrentContext: the game's VI manager ran\n");
    ctx->r2 = 0;
}

extern "C" void __osViSwapContext_recomp(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "__osViSwapContext: the game's VI manager ran\n");
}

extern "C" void __osTimerInterrupt_recomp(uint8_t* rdram, recomp_context* ctx) {
    fprintf(stderr, "__osTimerInterrupt: the game's VI manager ran\n");
}
