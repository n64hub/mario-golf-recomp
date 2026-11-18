// The settings: RecompFrontend's General, Controls, Graphics, Sound and Mods tabs, with its default bindings. Its own
// Graphics options carry what a recomp needs: Resolution and MS Anti-Aliasing (high resolution), Framerate
// (interpolation to the display's rate) and Aspect Ratio and HUD Placement (widescreen). The General tab adds the
// game's own option: Game Boy Golfers, which us.toml's hooks in character select (func_8005CA48) read through
// mg_gb_golfers.

#include <filesystem>

#include "recompui/config.h"
#include "util/file.h"
#include "mg.h"

namespace {
    constexpr const char* gb_golfers_id = "gb_golfers";

    enum class GbGolfers : uint32_t {
        Original,
        On,
    };
}

// Whether character select offers Kid, Sherry, Azalea and Joe (us.toml's hooks in func_8005CA48): 1 with Game Boy
// Golfers On.
extern "C" int mg_gb_golfers() {
    auto value = recompui::config::get_general_config().get_option_value(gb_golfers_id);
    return std::get<uint32_t>(value) == static_cast<uint32_t>(GbGolfers::On) ? 1 : 0;
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
        "Puts Kid, Sherry, Azalea and Joe, the golfers of Mario Golf for the Game Boy Color, at character select, with "
        "the stats the game itself holds for them. They look like the developers' test values: Kid drives 500 yards, "
        "Sherry 400, Azalea 300 and Joe 200, where a console's golfers bring their stats from the Game Boy save. "
        "<recomp-color primary>Original</recomp-color> leaves them out: on a console they come only through a "
        "Transfer Pak holding the Game Boy game, which this version doesn't have.",
        {{GbGolfers::Original, "Original"}, {GbGolfers::On, "On"}},
        GbGolfers::Original
    );
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();
    recompui::config::finalize();
}
