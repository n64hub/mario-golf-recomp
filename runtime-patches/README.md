# runtime-patches

This game's own changes to the submodules, applied on top of N64Recomp-browser's patches, in order of name, by
`apply.cmake` here when `CMakeLists.txt` configures and when `tools/build_n64recomp.sh` runs. A new change is a new file per submodule, `<submodule>-<n>-<what>.patch`, numbered after
that submodule's highest, with a line here.

- `N64ModernRuntime-1-pad-a-short-rom.patch`: a ROM whose hash doesn't match is padded with 0xFF to the next power of
  two and checked again, so the common 24 MB dump of Mario Golf's 32 MB cartridge (the 0xFF after its data left off)
  is taken as the cartridge it was cut from.
