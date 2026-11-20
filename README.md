# Mario Golf: Recompiled

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

A native PC port of the N64 *Mario Golf* (USA), made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64), with [RecompFrontend](https://github.com/N64Recomp/RecompFrontend)'s launcher
and menus): the game's own MIPS code translated to C and run natively, with RT64 drawing it. The symbols and the
patches' headers come from the [monde-lointain/mariogolf64](https://github.com/monde-lointain/mariogolf64)
decompilation. It is not an emulator.

The repo holds no ROM, no asset from the game and no data extracted from it: you supply your own ROM, and nothing in
git or CI is made from one except the symbol names and addresses in `syms/`.

**Status:** it boots and plays: the intro, the title, the menus and a Tournament on Toad Highlands, swings, the
ball's flight and the pause menu, with sound, and its Save and Quit writes the tournament to the game's SRAM. Up to
four players each play a controller of their own. Not played yet: the courses past the first, the other play modes,
a saved tournament continued after a restart, and several pads at once. On a window wider than 4:3 the course reaches
its edges and the HUD its sides, with the menus in the middle 4:3. The Game Boy Color link through the Transfer Pak is
not there, but the General tab's Game Boy Golfers option puts Kid, Sherry, Azalea and Joe at character select, with
the clubs the game has for them or, by Game Boy Golfers' Stats, its test values (Kid drives 500 yards). Its
Enhancements option, On by default, makes changes to how the game looks that a console doesn't, such as the sun's lens
flare fading out near the screen's edge. Original draws it all as a console does.

It plays in a browser too, at [n64hub.com/mario-golf](https://n64hub.com/mario-golf/) (`docs/web.md`), and the
desktop builds for Windows and Linux are on the [Releases](https://github.com/n64hub/mario-golf-recomp/releases) page.

## The ROM

*Mario Golf* (USA), as a big-endian `.z64`: the 32 MiB cartridge, SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`,
or the common 24 MiB dump of it, which leaves off the 0xFF the cartridge ends in and which the launcher pads back
(`runtime-patches/`). Other regions and revisions are not supported.

## Building

You need that ROM at `roms/mariogolf64.us.z64`. On Windows: LLVM's clang-cl, CMake 3.31 and Ninja, and a clang
with a MIPS backend plus ld.lld for `patches/`. On Linux: `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake
3.31 and Ninja.

From the repo's root in a bash shell:

```sh
git -c core.longpaths=true submodule update --init --recursive   # paths past Windows' 260 characters
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build MarioGolfRecompiled
```

With [ccache](https://ccache.dev) on `PATH` every compile goes through it, so a new build dir or another checkout
of the same code compiles only what changed since the last build on that computer (CMakeLists.txt says how).

On Linux the same lines take `clang` and `clang++` in place of `clang-cl`, and the system's `clang` and `ld.lld` for
`patches/`. `tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. Run
`build/MarioGolfRecompiled` (`.exe` on Windows) from the repo's root, pick the ROM with Load ROM, then Start Game.
`tools/release.sh`'s header says how a release is built.

`syms/` is written from the decomp's own build of your ROM: `tools/build_decomp.sh` builds it in Docker and brings
back its ELF, and `tools/gen_syms.sh` turns that into N64Recomp's symbols. Only a move of `lib/mariogolf64` needs them
again.

## License

GPL-3.0 (`LICENSE`), as the recompilations it follows.
