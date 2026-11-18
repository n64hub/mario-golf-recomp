# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into `MarioGolfRecompiled.html`
(the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and the saves kept in IndexedDB;
`lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn through WebGPU. The page is
built from your ROM like the desktop program, so it is never built in CI and never published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh`, the prebuilt shader list (`src/rt64_shaders.txt`, recorded by `tools/shader_plan.json`; the
list's header says how to record it again), the play plan (`tools/play_plan.json`) and the survey it shares with the
shader plan (`tools/shader_survey.js`). The rest is N64Recomp-browser's: `docs/porting.md` (WebGPU and the shaders,
prebuilt shader lists, Emscripten), `docs/playing.md` (playing and measuring a page), `docs/forks.md` and
`docs/findings.md`. The page is published at [n64hub.com/mario-golf](https://n64hub.com/mario-golf/).

## Build it

On Linux with Docker, with the ROM at `roms/mariogolf64.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/MarioGolfRecompiled.html` with its `.js`, `.wasm` (about 15 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser
page is built in, with ccache in `build/ccache-web`, so a rebuild after a patch takes a minute or two. The script's
header says each step. Two things differ from the desktop build:

- The patches library (`patches/`, built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh`
  builds, not the runtime's, which Emscripten would build for the browser.
- RT64 builds its pipelines from `src/rt64_shaders.txt` (`RT64_PREBUILT_SHADER_LIST`), in sections: the boot's before
  the game starts, and a course's the first time it draws.

The TLB's window at 0x02000000 (`src/game/tlb.cpp`) is the 16 MB that `RECOMP_KUSEG_WINDOW` puts right after RDRAM in
the wasm heap, so the page needs no more memory than any other.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `MarioGolfRecompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The plan

`tools/play_plan.json` (2-3 minutes) plays the game the way a player does, by the keyboard's defaults (Enter is Start,
Space A, WASD the stick): the intro's golfers, Start to the title and the main menu, A through the menus to a
Tournament on Toad Highlands, then four shots from its first hole's tee with the pause menu between. Where the game is
comes from which of its overlays are loaded, read from RDRAM by `tools/shader_survey.js` (its header has each screen's
overlays), so the plan presses on when a screen has come rather than by fixed times, and its waits on the course are on
the game's own frame count, 30 a second. Play it with N64Recomp-browser's `tools/play_page.mjs` (its
`docs/playing.md`).

Two more plans play what the regular one doesn't reach:

- `tools/play_plan_pads.json` plugs four stand-in pads in (N64Recomp-browser's `tools/gamepad_probe.js` as the
  preload), takes four players, has each port pick a golfer with its own controller and tee off in turn on Toad
  Highlands, and checks that the game counts four controllers with a Rumble Pak in each and that every pad has
  rumbled: the game buzzes a player's pad as their turn comes. Its header says what each step shows.
- `tools/play_plan_extended.json` (5 minutes at most) is the long play: a Tournament on Mario Star, opened with the
  GameShark's Have All Courses, played hole after hole at a shot and a Give Up each, for over three minutes of the
  game's busiest screens. The Leader Board that comes up between holes keeps the flyover's overlay loaded, so the
  plan presses past it and lets the next flyover play before it looks for the tee.

## The shader list

`src/rt64_shaders.txt` holds 375 shaders in nine sections: the boot's (the logos, the intro, the title and the
menus to a course), one for each of the six courses, from Toad Highlands to Mario Star, Mini-Golf's and the staff
credits'. It is recorded by code: the boot and Toad Highlands with `tools/shader_plan.json`, the other five courses,
Mini-Golf and the credits with `tools/shader_plan_courses.json`, which opens the courses on a new save by the
GameShark's Have All Courses and reaches the credits by setting a round to its credits state (the list's header says
what each section plays and how to record it again). A play that draws what the list lacks prints the shaders it
missed (`play_page.mjs` counts the misses), which go into the section it was in.

## How it plays

In Chrome RT64 builds the whole list behind the launcher, so nothing waits once Start Game is pressed, and on a
desktop GPU the page draws well above the display's rate at the canvas's size. Firefox builds the boot's section with
the game waiting before it starts and Toad Highlands' as the course first draws, under the page's panel, and on
Windows its compositor holds it to about 30 frames a second. Render to RAM is on, its copy landing a list late
(`src/main/main.cpp` says why): the card the last frame of a shot turns over on between shots is that copy, and no
play has seen the game's CPU write to a frame with the copy pending (RT64 prints a line each time it does).

The extended plan at 3840x2000, about five minutes on Mario Star, runs nothing out: Chrome's GPU process grew from 1.5
to 1.9 GB over it and Firefox's from 2.2 to 2.8 GB. Four stand-in pads play four golfers, each from the pad that
picked it, and two pads of one model pulled out and plugged in again at the tee get their ports back and rumble again
once the game has found their Rumble Paks, a few seconds on. On the desktop build every course, Mini-Golf, each play
mode and a whole Tournament round draw at 16:9, 21:9 and 4:3, and each hole's flyover whole at Framerate Display.
