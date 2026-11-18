// The settings: RecompFrontend's General, Controls, Graphics, Sound and Mods tabs, with its default bindings. Its own
// Graphics options carry what a recomp needs: Resolution and MS Anti-Aliasing (high resolution), Framerate
// (interpolation to the display's rate) and Aspect Ratio and HUD Placement (widescreen). The General tab adds the
// game's own option: Game Boy Golfers, which us.toml's hooks in character select (func_8005CA48) read through
// mg_gb_golfers and mg_gb_golfers_setup.

#include <cstdint>
#include <filesystem>

#include "recompui/config.h"
#include "util/file.h"
#include "mg.h"

namespace {
    constexpr const char* gb_golfers_id = "gb_golfers";

    enum class GbGolfers : uint32_t {
        Original,
        NormalStats,
        TestStats,
    };

    GbGolfers gb_golfers() {
        auto value = recompui::config::get_general_config().get_option_value(gb_golfers_id);
        return static_cast<GbGolfers>(std::get<uint32_t>(value));
    }

    // A halfword of the game's RAM at a KSEG0 address, as the runtime keeps it (each 32-bit word in the host's order).
    int16_t& ram_half(uint8_t* rdram, uint32_t addr) {
        return *reinterpret_cast<int16_t*>(rdram + ((addr - 0x80000000u) ^ 2));
    }

    // The game's two sets of driver stats for the four golfers (Kid, Sherry, Azalea and Joe, IDs 0xE-0x11), each
    // carry in yards, loft and curve. The normal ones are their 1W in the clubs table (D_800BB264, 14 clubs of 0x34
    // bytes a golfer: carry at +0, loft at +6, curve at +0x1A), what a golfer linked from the Game Boy plays with.
    // The test ones (D_800C2B48, D_800C2B50 and D_800C2B58: drives of 500, 400, 300 and 200) are what the debug flag's
    // path writes over that 1W. Both are read from RAM once, before that path has written anything, since a golfer's
    // 1W is written over again each time character select is built.
    constexpr uint32_t clubs = 0x800BB264, golfer_clubs = 14 * 0x34, first_id = 0xE;
    constexpr uint32_t test_carry = 0x800C2B48, test_loft = 0x800C2B50, test_curve = 0x800C2B58;
    bool stats_read = false;
    int16_t normal_stats[4][3];
    int16_t test_stats[4][3];
}

// Whether character select offers Kid, Sherry, Azalea and Joe (us.toml's hooks in func_8005CA48): 1 unless Game Boy
// Golfers is Original.
extern "C" int mg_gb_golfers() {
    return gb_golfers() != GbGolfers::Original ? 1 : 0;
}

// The same, at func_8005CA48's first test, before its path for the flag writes anything: with the golfers on, the
// stats that path gives them (D_800C2B48 on) are set to the option's, Normal Stats' or Test Stats'.
extern "C" int mg_gb_golfers_setup(uint8_t* rdram) {
    GbGolfers mode = gb_golfers();
    if (mode == GbGolfers::Original) {
        return 0;
    }
    if (!stats_read) {
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t driver = clubs + (first_id + i) * golfer_clubs;
            normal_stats[i][0] = ram_half(rdram, driver);
            normal_stats[i][1] = ram_half(rdram, driver + 6);
            normal_stats[i][2] = ram_half(rdram, driver + 0x1A);
            test_stats[i][0] = ram_half(rdram, test_carry + 2 * i);
            test_stats[i][1] = ram_half(rdram, test_loft + 2 * i);
            test_stats[i][2] = ram_half(rdram, test_curve + 2 * i);
        }
        stats_read = true;
    }
    const int16_t (*stats)[3] = mode == GbGolfers::TestStats ? test_stats : normal_stats;
    for (uint32_t i = 0; i < 4; i++) {
        ram_half(rdram, test_carry + 2 * i) = stats[i][0];
        ram_half(rdram, test_loft + 2 * i) = stats[i][1];
        ram_half(rdram, test_curve + 2 * i) = stats[i][2];
    }
    return 1;
}

void mg::init_config() {
    std::filesystem::path app_dir = recompui::file::get_app_folder_path();
    if (!app_dir.empty()) {
        std::filesystem::create_directories(app_dir);
    }

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = true;
    general_options.has_mouse_sensitivity = true;
    recomp::config::Config& general = recompui::config::create_general_tab(general_options);
    general.add_enum_option(
        gb_golfers_id,
        "Game Boy Golfers",
        "Puts Kid, Sherry, Azalea and Joe, the golfers of Mario Golf for the Game Boy Color, at character select. "
        "<recomp-color primary>Normal Stats</recomp-color> gives them the clubs the game has for them, drives of "
        "205-210 yards. <recomp-color primary>Test Stats</recomp-color> gives them the developers' test values the "
        "game also holds: Kid drives 500 yards, Sherry 400, Azalea 300 and Joe 200. "
        "<recomp-color primary>Original</recomp-color> leaves them out: on a console they come only through a "
        "Transfer Pak holding the Game Boy game, which this version doesn't have.",
        {{GbGolfers::Original, "Original"}, {GbGolfers::NormalStats, "NormalStats", "Normal Stats"},
         {GbGolfers::TestStats, "TestStats", "Test Stats"}},
        GbGolfers::Original
    );
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
}
