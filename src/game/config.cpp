// The settings: RecompFrontend's General, Controls, Graphics, Sound and Mods tabs, with its default bindings. Its own
// Graphics options carry what a recomp needs: Resolution and MS Anti-Aliasing (high resolution), Framerate
// (interpolation to the display's rate) and Aspect Ratio and HUD Placement (widescreen). The General tab adds the
// game's own options: Enhancements, the one switch for the changes to how the game looks that a console doesn't make,
// On by default (mg::enhancements(); its description lists them, and a new one goes under it rather than an option
// of its own), and Game Boy Golfers and Game Boy Golfers' Stats (disabled while the first is Original), which
// us.toml's hooks in character select (func_8005CA48) read through mg_gb_golfers and mg_gb_golfers_setup, and
// mg_gb_golfers_clubs works out the four golfers' shots once they are in.

#include <cstdint>
#include <filesystem>

#include "recomp.h"
#include "recompui/config.h"
#include "util/file.h"
#include "mg.h"

namespace {
    constexpr const char* enhancements_id = "enhancements";
    constexpr const char* gb_golfers_id = "gb_golfers";
    constexpr const char* gb_golfer_stats_id = "gb_golfer_stats";

    enum class Enhancements : uint32_t {
        Original,
        On,
    };

    enum class GbGolfers : uint32_t {
        Original,
        On,
    };

    enum class GbGolferStats : uint32_t {
        Normal,
        Test,
    };

    bool gb_golfers_on() {
        auto value = recompui::config::get_general_config().get_option_value(gb_golfers_id);
        return std::get<uint32_t>(value) == static_cast<uint32_t>(GbGolfers::On);
    }

    GbGolferStats gb_golfer_stats() {
        auto value = recompui::config::get_general_config().get_option_value(gb_golfer_stats_id);
        return static_cast<GbGolferStats>(std::get<uint32_t>(value));
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

// Whether character select offers Kid, Sherry, Azalea and Joe (us.toml's hooks in func_8005CA48): 1 with Game Boy
// Golfers On.
extern "C" int mg_gb_golfers() {
    return gb_golfers_on() ? 1 : 0;
}

// The same, at func_8005CA48's first test, before its path for the flag writes anything: with the golfers on, the
// stats that path gives them (D_800C2B48 on) are set to Game Boy Golfers' Stats', Normal or Test.
extern "C" int mg_gb_golfers_setup(uint8_t* rdram) {
    if (!gb_golfers_on()) {
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
    const int16_t (*stats)[3] = gb_golfer_stats() == GbGolferStats::Test ? test_stats : normal_stats;
    for (uint32_t i = 0; i < 4; i++) {
        ram_half(rdram, test_carry + 2 * i) = stats[i][0];
        ram_half(rdram, test_loft + 2 * i) = stats[i][1];
        ram_half(rdram, test_curve + 2 * i) = stats[i][2];
    }
    return 1;
}

// The game's own working-out of how a club's shot flies (func_80043EA4, its argument the club's index in the clubs
// table, D_800BB264 less 0xC, 0x34 bytes a club): it fills the club's words at +0x18, +0x1C and +0x20 and its halves at
// +0x28, +0x2A and +0x2C, which a swing reads, from its carry, loft and curve.
extern "C" void func_80043EA4(uint8_t* rdram, recomp_context* ctx);

// At the end of func_8005CA48 (us.toml), with the golfers on: each of the four's 14 clubs worked out by func_80043EA4,
// as the Transfer Pak's link does for a golfer it reads (func_80044254). The game works out only the first 14 golfers'
// clubs as it boots (func_800444B8, which copies them from a table of its own), so without this the four's clubs fly
// nowhere: a swing moved the ball a few hundredths of a yard, and it stayed on the tee all round (issue 90). Each
// time, since Game Boy Golfers' Stats may have changed their drivers: 40-80 ms, once as character select opens. A copy
// of the context makes the calls, so the function's registers are as they were. The game's own table isn't this
// function's answer at character select: run there on the first four golfers' clubs, it came within 0.1% of the table's
// words, but its halves were others (Plum's driver -216, 6 and -28 against 10, 10 and 15), so it isn't the four's
// clubs as the developers left the others'; it is what the link gives a linked golfer, made the same way.
extern "C" void mg_gb_golfers_clubs(uint8_t* rdram, recomp_context* ctx) {
    if (!gb_golfers_on()) {
        return;
    }
    recomp_context call = *ctx;
    for (uint32_t club = first_id * 14; club < (first_id + 4) * 14; club++) {
        call.r4 = club;
        func_80043EA4(rdram, &call);
    }
}

bool mg::enhancements() {
    auto value = recompui::config::get_general_config().get_option_value(enhancements_id);
    return std::get<uint32_t>(value) == static_cast<uint32_t>(Enhancements::On);
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
        enhancements_id,
        "Enhancements",
        "Changes to how the game looks that a console doesn't make, none of them to how it plays: the sun's lens flare "
        "fades out as the sun nears an edge of the screen, where the game takes it away at once, and the black round the "
        "iris between screens is solid, where a faint line of the picture showed through it beside Koopa Park's shell. "
        "<recomp-color primary>Original</recomp-color> draws it all as a console does.",
        {{Enhancements::Original, "Original"}, {Enhancements::On, "On"}},
        Enhancements::On
    );
    general.add_enum_option(
        gb_golfers_id,
        "Game Boy Golfers",
        "Puts Kid, Sherry, Azalea and Joe, the golfers of Mario Golf for the Game Boy Color, at character select, "
        "with the stats Game Boy Golfers' Stats picks. <recomp-color primary>Original</recomp-color> leaves them out: "
        "on a console they come only through a Transfer Pak holding the Game Boy game, which this version doesn't "
        "have.",
        {{GbGolfers::Original, "Original"}, {GbGolfers::On, "On"}},
        GbGolfers::Original
    );
    general.add_enum_option(
        gb_golfer_stats_id,
        "Game Boy Golfers' Stats",
        "The Game Boy golfers' stats. <recomp-color primary>Normal</recomp-color>: the clubs the game has for them, "
        "drives of 205-210 yards, about as long as the other golfers. "
        "<recomp-color primary>Test</recomp-color>: the developers' test values the game also holds, Kid driving 500 "
        "yards, Sherry 400, Azalea 300 and Joe 200.",
        {{GbGolferStats::Normal, "Normal"}, {GbGolferStats::Test, "Test"}},
        GbGolferStats::Normal
    );
    general.add_option_disable_dependency(gb_golfer_stats_id, gb_golfers_id, GbGolfers::Original);
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
}
