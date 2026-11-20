# CLAUDE.md

A static recompilation of the N64 *Mario Golf* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64, from
the monde-lointain/mariogolf64 decompilation. Almost all of it is written by AI agents, so this file is what the next
one needs: the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, written from the decomp's own build of the ROM (`tools/build_decomp.sh`, then
  `tools/gen_syms.sh` and `tools/gen_syms.py`, whose header says how), never edited by hand: every code section of the
  game with its ROM offset and VRAM, the functions named as the decomp names them.
- `us.toml`: N64Recomp's configuration, with the game's hooks; `tools/recompile.sh` writes `RecompiledFuncs/` with it,
  and the audio microcode with `n_aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/mariogolf64`'s headers, recompiled into PatchesLib, where a patch
  to the game's C goes (`patches/Makefile`'s header says how they build). `ui_funcs.h`, `patch_helpers.h` and
  `recompui_event_structs.h` are RecompFrontend's, which includes the first from the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the code sections and patches it gives the
  runtime (`src/main/register_overlays.cpp`, `src/main/register_patches.cpp`), the TLB (`src/game/tlb.cpp`), the
  libultra functions the runtime lacks (`src/game/libultra.cpp`), the settings (`src/game/config.cpp`), and on Windows
  a stack trace on a crash (`src/main/crash_trace.cpp`).
- `lib/`: `mariogolf64` (the decomp, for the patches' headers and `syms/`), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release tag.
  The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three, and `CMakeLists.txt` then this game's own changes to them
  (`runtime-patches/`, its README lists them).
- `docs/web.md`: the web page: `tools/build_web.sh`, its prebuilt shader list (`src/rt64_shaders.txt`,
  `tools/shader_plan.json`, `tools/record_shaders.sh`), its play plans (`tools/play_plan*.json`) and the survey they
  share (`tools/shader_survey.js`).
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing from
  the decomp's assets or its build is committed but `syms/`. The icon (`icons/icon.svg`, the launcher's thumbnail and
  the program's icon by `tools/icon.py`) is drawn for the project from plain shapes: no logo, character or picture from
  the game.
- The desktop releases are built where the ROM is, by `tools/release.sh` (its header says how), and published to
  GitHub Releases with their notes in `releases/`.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus modern enhancements: each an option with an Original value that plays as the console does, none
  changing how the game plays, but for what a console gets only through an accessory the recompilation lacks, made
  from the game's own data: the Game Boy Golfers option (`us.toml`). A change to how the game looks goes under the
  General tab's one Enhancements option, On by default (`src/game/config.cpp`), not an option of its own. USA only.
- Code from any other Mario Golf project may be read for what it changes, but nothing that isn't the decomp's (MIT) or
  CC0 is copied.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

- **The ROM.** The cartridge is 32 MB, its last 8 MB 0xFF; the common dump leaves those off, and the runtime pads a
  ROM short of a power of two back before it checks the hash (`runtime-patches/`).
- **The code.** `.main` (ROM 0x1000, VRAM 0x80025C00, the entry point) is stored raw in the ROM, and the runtime maps
  what lies in ROM 0x1000 plus 1 MB at start. The rest is 16 overlays, listed in a table at ROM 0x91360 (VRAM
  0x800B5F58), 0x28 bytes each, which the game's loader (func_800260C4) copies with one nuPiReadRom each; the hook there
  (`us.toml`) tells the runtime what is about to be copied (`mg_load_overlays`, `src/main/register_overlays.cpp`). Many
  share a VRAM. ROM 0x11BA50 is a gap in the decomp's layout (its `overlay_gap`), data no code is read from.
- **The TLB.** The game maps one 128 KB window at 0x02000000 to one object's buffer after another and works on each
  through it (the intro's models among them): `src/game/tlb.cpp` copies a mapping's pages in and
  writes what changed back (its header says how), with two callbacks the runtime gains for it
  (`runtime-patches/N64ModernRuntime-2-tlb-callbacks.patch`: osVirtualToPhysical of a window address, and a writeback
  before each RSP task). `RECOMP_KUSEG_WINDOW` gives the recompiled code that window, natively and on the page
  (`CMakeLists.txt`).
- **Spins.** The runtime runs one game thread at a time and switches only when the running one calls into it, where a
  console's interrupts preempt any thread. Four loops spin on what another thread changes: NuSystem's idle thread
  waiting for an idle function the game never sets, the main thread's wait for a game mode that the graphics callback
  sets (func_80029C34), nuGfxTaskAllEndWait on its task count, and Training's wait of three retraces as its hole
  changes (func_80069D08). Each pass yields (`us.toml`). Without the first three the game hangs on a black screen at
  boot, and without the last Training froze on its course screen. A new hang with every other game thread waiting in `osRecvMesg` is another such
  loop: `--stacks <seconds>` prints every thread's stack that often on Windows (`src/main/crash_trace.cpp`), which
  finds it. A load the game spreads over frames is done here before the next one is drawn: at Ring Shot's stage select
  the board's slats close over a change of stage's load and turn to open when it ends, so the loader waits there until
  they are shut (`src/game/loader.cpp`), or they flashed for a frame.
- **Renames.** The game has functions of its own named `sin`, `cos`, `tan`, `atan`, `atan2` and `main`, which N64Recomp
  renames (`us.toml`), and a call of address 0 that never runs, nopped.
- **Microcodes.** Graphics: F3DEX2, which RT64 knows. Audio: `n_aspMain`, recompiled by RSPRecomp from
  `n_aspMain.us.toml`. librecomp's vector unit uses SSE4.1, so `RecompiledFuncs` builds with `-march=nehalem` on
  x86-64.
- **Sound.** libmus, at 32000 Hz, makes 552 samples a retrace and leaves its pace to the audio interface's FIFO, which
  the runtime never reports full: `patches/audio.c` sizes each frame by what is queued, without which the queue grew to
  `queue_samples`' 100 ms catch-up and clicked about four times a second. A change to the sound reads what is queued
  through a play (a log of `get_queued_audio_bytes` beside each `queue_samples`, on the sound device: SDL's disk driver
  plays slower than real time, so it never shows a queue that runs dry).
- **Saves.** 256 Kbit of SRAM. `main.cpp` declares `SaveType::Sram`; the runtime keeps it in
  `saves/mariogolf64.n64.us.1.0.bin`.
- **The Transfer Pak.** The game looks for one for its link with the Game Boy Color game; `src/game/libultra.cpp`
  answers that none is plugged in, as on a console with none. The link (overlay 15) only reads the four Game Boy
  golfers into RAM for the session, never into the save. The game has the four of its own, which character select
  offers in their place behind debug flag 0x54, with test values for their drivers: the General tab's Game Boy Golfers
  option answers that flag on there alone (`us.toml`), since its other reader unlocks every golfer, and Game Boy
  Golfers' Stats keeps the drivers the clubs table has for them (Normal) or the test values (Test). A swing reads
  what the game works out of each club's carry, loft and curve (func_80043EA4), which it copies in at boot for the
  first 14 golfers only and the link works out for a golfer it reads: `src/game/config.cpp` works out the four's at
  character select, without which their shots went nowhere.
- **Threads** (`main.cpp` names them): 0 the PI manager or the VI manager (by priority), 1 idle, 3 the game's main at
  priority 10, 4 NuSystem's graphics thread, 5 its task manager or the controllers (by priority), 17-19 the
  scheduler's threads.
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L, Enter
  Start, the arrows the C buttons, I, J, K and L the D-pad. The game takes up to four players, each on a controller of
  their own: until players are assigned, each pad plays its own port in the order it was plugged in and the keyboard
  plays port 1 (`RECOMPINPUT_SINGLE_PLAYER_UNTIL_ASSIGNED`, `RECOMPINPUT_PADS_IN_ORDER`, `CMakeLists.txt`). At
  character select whichever controller picks first is 1P, the next 2P and so on, and the game plays each player from
  the controller that picked, and buzzes its Rumble Pak as that player's turn comes. The main menu zooms in over the
  title for about half a second after overlay 3 loads, and drops a press made then, so a script waits for the zoom
  (or presses until the screen it wants has come) rather than pressing at a fixed time.
- **The clock.** The game never calls `osSetTime`, which isn't linked, and only a debug timing meter reads
  `osGetTime`: func_8002DC10 and the functions after it time the frame's parts in D_800B7734-D_800B7740, which
  0x80031340-0x8003137C draw as bars when the debug flag D_800B67C0 is on (a code entered at the title, overlay 8's
  table at ROM 0x14F994). So time spent in the launcher reaches nothing a player sees.
- **Graphics.** RecompFrontend's Graphics tab defaults to Resolution Auto and Framerate Display, so RT64 interpolates
  to the display's rate. The game's own picture is 4:3: RT64's Expand widens the course, which `src/game/border.cpp`
  draws through the game's own 8-pixel border to the window's edges (on a hole and in the opening), `patches/cull.c` culls the
  course's terrain at the wide picture's width rather than the 4:3 frame's, `src/game/hud.cpp` puts the HUD at the
  wide picture's sides, the hole's waving sign in a matrix group whose vertices RT64 moves between the game's frames, and `src/game/backdrop.cpp` draws the sun's beam and glare on the sky on past the 4:3 frame's
  sides, where the game's scissor cuts them, and while any of them is in the picture, where the course fills the screen, puts them where the sun is between whole pixels, by moving their texture, rather than on the game's whole one, puts the sky in a matrix group whose clouds RT64 slides
  between the game's frames, and draws the far hills on to the wide picture's sides at the width they have in the 4:3
  frame, where RT64 stretched them across it, `src/game/ball.cpp` draws the ball's sprites, the stars in the view of the cup and the sun's lens flare on into
  the wide picture's sides, where the game cuts them at the screen's, puts the ball's sprites and the flare's circles
  where the game works them out, to a quarter of a pixel, rather than on the whole pixels it truncates them to, which
  RT64 draws there only with `RT64_SUBPIXEL_TEXRECTS` (`CMakeLists.txt`), or the circles stepped against the sun's light
  on the game's own frames, and with Enhancements On draws the flare on to the screen's edge, where the game drops it 16
  pixels in (hooks in func_8008D3F4, `us.toml`), its circles fading out before it, `src/game/aim.cpp` puts the red pin flag in the sky and the landing point's label where their points are
  across, to a quarter pixel, not the whole one the game truncates them to, or they slid in whole console pixels a game
  frame as the view turned, and draws them on into the wide picture's sides as `ball.cpp` draws its sprites, where the
  3D's scissor cut them 8 pixels inside the 4:3 frame (RT64 pairs the flag between game frames as it moves fast only
  with `runtime-patches/rt64-2-pair-a-lone-look-farther.patch`, or it stepped as the view turned fast), and `src/game/screen.cpp` scales the title and the menus, 2D screens
  RT64 keeps at 4:3 inside the game's own 8-pixel border, to the picture's full height, the tile wipe between the title
  and the main menu included, and `src/game/credits.cpp`
  the end credits' last picture the same way, which RT64 stretched across the window, `src/game/iris.cpp` draws the iris wipe between screens at its
  own shape across the wide picture, with Enhancements On its black solid past the mask's edges, and `src/game/card.cpp` draws the old shot a transition fades out or opens on across it and to its top and bottom, both where
  RT64 stretched them, and `src/game/slats.cpp` closes Ring Shot's stage-select slats over the whole picture, plain
  planks past its 4:3 picture of Mario, where they covered only the 4:3 frame inside the border (their headers say
  how). The pin's flagstick is drawn from screen positions the game works out
  itself (G_MODIFYVTX), which RT64 widens with the course only with `RT64_EXPAND_SCREEN_VERTICES` (`CMakeLists.txt`),
  or it stands away from the hole and its marker, and draws them between game frames only with
  `RT64_INTERPOLATE_SCREEN_VERTICES`, or it shakes at its foot at a display rate above 30. Ring Shot's rings sample a
  32x32 map the game rebuilds each frame from strips of the picture, often fewer than its 32 rows, which RT64 draws from
  what RAM holds only with `RT64_TILE_COPY_TMEM_FALLBACK`, or they draw solid black now and then. The course's terrain is
  never drawn at its coarsest level of detail (`us.toml`): its edges left cracks beside finer blocks at a high
  resolution. Its players take turns on one view, never a split screen. `src/game/golfer.cpp` puts each golfer's
  joints in a matrix group of its own, each joint's matrix blended whole between the game's frames and its position
  always, or Wario's upper arm as he shakes his fist on the Play Modes screen swung through his belly (RT64 blends a
  matrix split into rotation and scale) or jumped there ahead of his glove (it blends a position only while it keeps
  its pace). `src/game/billboard.cpp` puts each of the course's billboards (its trees and Bomb-ombs, one quad scaled to
  each) in a matrix group of its scale, so RT64 pairs one between the game's frames only with one of its own size, or
  a Bomb-omb coming into view as a tree left it was drawn halfway to the tree's size for a frame. `src/game/callout.cpp`
  puts each letter of the big words a hole or a shot ends on (HOLE-IN-ONE!, NICE SHOT!, one quad shared by every
  letter) in a matrix group of its own, keyed by its texture, or between the game's frames letters went missing or
  stood doubled as the word squashed and bounced.
  RT64 blends a vertex's shade across a triangle linearly on the screen, as the RDP does, not with perspective
  (`runtime-patches/rt64-1-shade-across-the-screen.patch`): the sky's three triangles run from overhead to the
  horizon, and with perspective their deep-blue overhead shade reached down to the far hills, where a console's sky is
  pale.
- **Testing.** `--boot` starts the game from the launcher's first frame once the ROM is stored, `--window <w>x<h>`
  sizes the window, and `--input <file>` plays port 1 from a script of timed button states (`main.cpp` says the
  format), for unattended runs. A swing is three presses of A: start, power, impact. `tools/frame_plan.txt` walks
  every screen a player meets (the intro, the menus, each mode, the end credits) as a plan for frame dumps of the
  desktop build, waiting on RAM so its runs keep step; its header says its steps, how it moves between modes and what
  each wait reads. A scene a fault turns up in that it doesn't reach is added to it with the fix. `tools/soak_plan.txt`
  plays one round of Tournament, Ring Shot, Training and Mini-Golf in three minutes for a soak that fails on a crash
  or a hang, each step waiting on the game's RAM (its header lists the addresses: the menu screen, the Play Modes
  cursor, the pause and its menu). `tools/save_plan.txt` and `tools/load_plan.txt` are the desktop save check: a
  Tournament round left by Save and Quit, and after a restart, with the saves behind symlinks, that round read back
  off the SRAM into its copy in RAM at 0x8012F720 (their headers say which words).
