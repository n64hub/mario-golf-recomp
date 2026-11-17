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
  changing how the game plays. USA only.
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
  finds it.
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
  answers that none is plugged in, as on a console with none.
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
  to the display's rate. The game's own picture is 4:3: RT64's Expand widens the course, `patches/cull.c` culls the
  course's terrain at the wide picture's width rather than the 4:3 frame's, `src/game/hud.cpp` puts the HUD at the
  wide picture's sides, and `src/game/backdrop.cpp` draws the sun's beam and glare on the sky on past the 4:3 frame's
  sides, where the game's scissor cuts them, and while any of them is in the picture, and puts the sky in a matrix group whose clouds RT64 slides
  between the game's frames, `src/game/ball.cpp` draws the ball's sprites and the stars in the view of the cup on into
  the wide picture's sides, where the game cuts them at the screen's, and `src/game/title.cpp` scales the title's painting, a 2D screen
  RT64 keeps at 4:3, to the picture's full height, `src/game/iris.cpp` draws the iris wipe between screens at its
  own shape across the wide picture, and `src/game/card.cpp` keeps the old shot a dissolve fades out at 4:3, both where
  RT64 stretched them (their headers say how). The pin's flagstick is drawn from screen positions the game works out
  itself (G_MODIFYVTX), which RT64 widens with the course only with `RT64_EXPAND_SCREEN_VERTICES` (`CMakeLists.txt`),
  or it stands away from the hole and its marker. The course's terrain is never drawn at its coarsest level of detail (`us.toml`):
  its edges left cracks beside finer blocks at a high resolution. Its players take turns on one view, never a split screen,
  so N64Recomp-browser's docs/new-recomp.md item 1a (split screen) isn't in its row.
- **Testing.** `--boot` starts the game from the launcher's first frame once the ROM is stored, `--window <w>x<h>`
  sizes the window, and `--input <file>` plays port 1 from a script of timed button states (`main.cpp` says the
  format), for unattended runs. A swing is three presses of A: start, power, impact.
