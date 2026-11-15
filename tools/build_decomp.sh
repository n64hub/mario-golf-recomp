#!/usr/bin/env bash
# Builds the decomp (lib/mariogolf64's commit) from your ROM and brings its ELF back for tools/gen_syms.sh.
# It runs in tools/docker/Dockerfile.decomp's image with Docker, here or on a Linux host over ssh (BUILD_HOST). Never
# in CI: the build needs the ROM.
#
# The decomp builds the 32 MB ROM: the cartridge's 24 MB, then 0xFF to 32 MB, as a 32 MB dump of it reads. A 24 MB
# dump is padded so before its SHA-1 is checked. `make setup` makes the venv (splat) and downloads KMC GCC,
# `make extract` splits the ROM with splat, and `make` builds it and checks the ROM it links against
# mariogolf64.md5. Into build/decomp/: mariogolf64.elf. Nothing from it is committed but what tools/gen_syms.sh
# writes from it.
#
# Usage: [BUILD_HOST=<ssh host>] [DECOMP_DIR=<folder under ~, default mariogolf64-decomp>] tools/build_decomp.sh [roms/mariogolf64.us.z64]
set -euo pipefail
cd "$(dirname "$0")/.."
rom="${1:-roms/mariogolf64.us.z64}"
sha1="e2c4e7a905b29529b49a1619a401fe699224829b"
padded="$(mktemp)"
trap 'rm -f "$padded"' EXIT
cp "$rom" "$padded"
size=$(wc -c < "$padded")
(( size <= 0x2000000 )) && head -c $((0x2000000 - size)) /dev/zero | tr '\0' '\377' >> "$padded"
[[ "$(sha1sum "$padded" | cut -d' ' -f1)" == "$sha1" ]] || { echo "$rom isn't Mario Golf (USA) as a .z64" >&2; exit 1; }
commit="$(git ls-tree HEAD lib/mariogolf64 | awk '{print $3}')"
image="mariogolf64-decomp:$(git hash-object tools/docker/Dockerfile.decomp | cut -c1-12)"
work="${DECOMP_DIR:-mariogolf64-decomp}/${commit:0:12}"

if [[ -n "${BUILD_HOST:-}" ]]; then
    run() { ssh "$BUILD_HOST" "cd && $1"; }
    put() { scp -q "$1" "$BUILD_HOST:$2"; }
    get() { scp -q "$BUILD_HOST:$1" "$2"; }
else
    run() { (cd ~ && bash -c "$1"); }
    put() { cp "$1" ~/"$2"; }
    get() { cp ~/"$1" "$2"; }
fi

run "mkdir -p $work.docker"
put tools/docker/Dockerfile.decomp "$work.docker/Dockerfile"
run "docker image inspect $image > /dev/null 2>&1 || docker build -q -t $image $work.docker > /dev/null"
run "[ -d $work/.git ] || git clone -q https://github.com/monde-lointain/mariogolf64 $work
     git -C $work checkout -q $commit"
put "$padded" "$work/baserom.z64"
run "docker run --rm --name mariogolf64-decomp-${commit:0:12} -u \$(id -u):\$(id -g) -e HOME=/tmp -v \$HOME/$work:/mg -w /mg $image bash -c '
    set -e
    [ -x venv/bin/python3 ] && [ -d tools/cc ] || make setup > setup.log 2>&1 || { tail -30 setup.log; exit 1; }
    [ -d asm ] || make extract > extract.log 2>&1 || { tail -30 extract.log; exit 1; }
    make -j\$(nproc) > build.log 2>&1 || { tail -30 build.log; exit 1; }
    echo \"$sha1  build/mariogolf64.z64\" | sha1sum -c'"

mkdir -p build/decomp
get "$work/build/mariogolf64.elf" build/decomp/
echo "build/decomp/: the decomp's ELF at ${commit:0:12}, matching the ROM; next, tools/gen_syms.sh"
