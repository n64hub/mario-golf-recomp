# runtime-patches

This game's own changes to the submodules, applied on top of N64Recomp-browser's patches, in order of name, by
`apply.cmake` here when `CMakeLists.txt` configures and when `tools/build_n64recomp.sh` runs. A new change is a new file per submodule, `<submodule>-<n>-<what>.patch`, numbered after
that submodule's highest, with a line here.

- `N64ModernRuntime-1-pad-a-short-rom.patch`: a ROM whose hash doesn't match is padded with 0xFF to the next power of
  two and checked again, so the common 24 MB dump of Mario Golf's 32 MB cartridge (the 0xFF after its data left off)
  is taken as the cartridge it was cut from.
- `N64ModernRuntime-2-tlb-callbacks.patch`: ultramodern's `TlbCallbacks`, which the game sets (`src/game/tlb.cpp`):
  osVirtualToPhysical asks it for a KUSEG address the game has mapped, and each RSP task first lets it write the
  pages mapped there back to RDRAM, where the RSP reads them.
- `rt64-1-textured-rects-across-to-a-quarter-pixel.patch`: with `RT64_INTERPOLATE_RECT_SCROLLS`, a texture rectangle
  (but in fill or copy mode) is drawn across at the quarter pixel the game gives its edges in every frame, not on the
  whole pixel it rounds up to; down it stays on whole pixels. The game's sprites over the course (the
  pin's flag in the sky, `src/game/aim.cpp`, and the ball, `src/game/ball.cpp`) are put where their points are to a
  quarter pixel, and slid between the game's frames from a quarter pixel to a whole one, each game frame began off
  where the last had ended, the flag hopping as the view turned.
