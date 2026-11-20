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
- `rt64-1-shade-across-the-screen.patch`: RT64's 3D draws blend a vertex's shade across a triangle linearly on the
  screen (`noperspective`), as the RDP does, which corrects only texture coordinates for perspective. The sky is three
  triangles from overhead (shade 255, 231, 144) to the horizon (192, 64, 0), which come out deep blue overhead and
  pale at the horizon, and blended with perspective the deep blue reached down to the far hills, where a console's sky
  is pale: as the camera looked low (Luigi's Garden's Mini-Golf flyover), the gaps between the
  hills along the picture's top showed dark blue, and the sky over a tee was deep blue all over.
- `rt64-2-pair-a-lone-look-farther.patch`: RT64_INTERPOLATE_RECT_SCROLLS pairs a texture rectangle with one of the
  game frame before that moved less than its longer side, so a glyph of a font is never paired with the same letter
  elsewhere; a rectangle whose look no other rectangle of either frame shares now pairs with its one look-alike up to
  four times that far. The red pin flag in the sky (`src/game/aim.cpp`), 8 by 16 pixels, moves 22 to 26 pixels a game
  frame as the view turns fast, and unpaired it held still between the game's frames and stepped once each against the
  course turning smoothly behind it (issue 181).
