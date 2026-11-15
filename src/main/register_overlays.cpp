#include "mg.h"
#include "../../RecompiledFuncs/recomp_overlays.inl"

#include "librecomp/overlays.hpp"

// The game's code sections, from N64Recomp's section table: .main, mapped at boot with the 1 MB after ROM 0x1000, and
// the overlays, each mapped when the game copies it out of ROM (mg_load_overlays below).
void mg::register_overlays() {
    recomp::overlays::overlay_section_table_data_t sections {
        .code_sections = section_table,
        .num_code_sections = ARRLEN(section_table),
        .total_num_sections = num_sections,
    };

    recomp::overlays::overlays_by_index_t overlays {
        .table = overlay_sections_by_index,
        .len = ARRLEN(overlay_sections_by_index),
    };

    recomp::overlays::register_overlays(sections, overlays);
}

// The hook where the game copies code out of ROM (us.toml): the code sections lying wholly in the size bytes of ROM at
// rom are now at ram. The game's overlay loader (func_800260C4, from its table at 0x800B5F58) reads each overlay with
// one nuPiReadRom of the whole of it; a read of data maps nothing.
extern "C" void mg_load_overlays(uint8_t* rdram, uint32_t rom, uint32_t ram, uint32_t size) {
    (void)rdram;
    load_overlays(rom, (int32_t)ram, size);
}
