/* The natives the patches call: each address is a dummy N64Recomp turns into a call of the C function of that name,
   the runtime's <libultra function>_recomp (patches.h renames the libultra calls) or one of src/game's. */
__start = 0x80000000;

osRecvMesg_recomp = 0x8F000004;
osEPiStartDma_recomp = 0x8F000008;
mg_cull_width_scale = 0x8F00000C;
mg_gb_golfers = 0x8F000010;
