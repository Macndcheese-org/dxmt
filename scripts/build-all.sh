#!/usr/bin/env bash
#
# Build the whole DXMT runtime MacNdCheese ships, from two cores that are
# deliberately kept apart:
#
#   this tree     (3Shain-based)  -> dxgi, d3d10core, d3d11, d3d12, winemetal
#   d3d9-core/    (GameSir-based) -> d3d9, winemetal9
#
# The two never meet. Each DLL statically links its own copy of the core, so the
# only thing they could have shared is the unix bridge -- and that is why the
# GameSir tree builds its bridge as winemetal9 rather than winemetal. The bridge
# dispatches by raw numeric index, so a PE and a .so from different trees cannot
# be mixed; giving the second one its own name makes that impossible by
# construction instead of by convention.
#
# Nothing from d3d9-core/ is used for d3d10/d3d11/d3d12/dxgi, and nothing from
# this tree is used for d3d9.
#
# Usage:
#   scripts/build-all.sh <wine-install-tree> [output-dir]
#
# <wine-install-tree> is a Wine install layout (or a symlink farm shaped like
# one) providing:
#   lib/wine/x86_64-windows/{libwinecrt0,libntdll,libdbghelp}.a
#   lib/wine/i386-windows/{libwinecrt0,libntdll,libdbghelp}.a
#   lib/wine/x86_64-unix/{winemac,ntdll}.so
#   bin/winebuild
#
# Output layout mirrors a Wine build tree, ready to drop in:
#   <out>/x86_64-windows/*.dll
#   <out>/i386-windows/*.dll
#   <out>/x86_64-unix/*.so

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WINE="${1:?usage: build-all.sh <wine-install-tree> [output-dir]}"
OUT="${2:-$ROOT/dist}"
D3D9_CORE="$ROOT/d3d9-core"

WINE="$(cd "$WINE" && pwd)"
[ -x "$WINE/bin/winebuild" ] || { echo "no winebuild under $WINE/bin" >&2; exit 1; }

MESON_COMMON=(
  -Dwine_install_path="$WINE"
  -Dwine_build_path=
  --buildtype release
)

# d3d9 must NOT carry the "Wine builtin DLL" marker. MacNdCheese loads it as
# NATIVE (the backend adds a "d3d9=n" override on Apple GPUs), and wine refuses
# to load a builtin-marked PE as native -- the DLL is found, load_dll returns
# STATUS_DLL_NOT_FOUND anyway, and the game dies in the loader with c0000135
# before any of its own code runs. The option defaults to true, so a build dir
# configured without this looks fine and produces a d3d9 that cannot be loaded.
D3D9_ONLY=(
  -Dwine_builtin_dll=false
)

# Configure once per (tree, arch); reconfigure is a no-op when nothing changed.
setup() {
  local tree="$1" builddir="$2" crossfile="$3"; shift 3
  if [ -d "$tree/$builddir" ]; then
    return 0
  fi
  ( cd "$tree" && meson setup --cross-file "$crossfile" "${MESON_COMMON[@]}" "$@" "$builddir" )
}

build() {
  local tree="$1" builddir="$2"; shift 2
  ninja -C "$tree/$builddir" "$@"
}

echo "==> MacNdCheese core (3Shain): dxgi, d3d10core, d3d11, d3d12, winemetal"
setup "$ROOT" build.w64 build-win64.txt
setup "$ROOT" build.w32 build-win32.txt
build "$ROOT" build.w64
build "$ROOT" build.w32

echo "==> d3d9-core (GameSir): d3d9, winemetal9"
setup "$D3D9_CORE" build.w64 build-win64.txt -Denable_nvapi=false -Denable_nvngx=false "${D3D9_ONLY[@]}"
setup "$D3D9_CORE" build.w32 build-win32.txt -Denable_nvapi=false -Denable_nvngx=false "${D3D9_ONLY[@]}"
# setup() is a no-op on an existing build dir, so an older one keeps whatever it
# was configured with; re-assert the option every run.
for b in build.w64 build.w32; do
  meson configure "$D3D9_CORE/$b" "${D3D9_ONLY[@]}" >/dev/null 2>&1 || true
done
build "$D3D9_CORE" build.w64 dxmt-d3d9
build "$D3D9_CORE" build.w32 dxmt-d3d9

echo "==> staging into $OUT"
mkdir -p "$OUT/x86_64-windows" "$OUT/i386-windows" "$OUT/x86_64-unix"

# The .so is 64-bit only; a 32-bit PE reaches it through the WoW64 thunks.
copy_if() { [ -f "$1" ] && cp "$1" "$2" && echo "    $(basename "$1") -> ${2#$OUT/}"; }

for f in "$ROOT"/build.w64/src/*/*.dll;              do copy_if "$f" "$OUT/x86_64-windows/"; done
for f in "$ROOT"/build.w32/src/*/*.dll;              do copy_if "$f" "$OUT/i386-windows/";   done
for f in "$ROOT"/build.w64/src/*/unix/*.so;          do copy_if "$f" "$OUT/x86_64-unix/";    done
for f in "$D3D9_CORE"/build.w64/src/*/*.dll;         do copy_if "$f" "$OUT/x86_64-windows/"; done
for f in "$D3D9_CORE"/build.w32/src/*/*.dll;         do copy_if "$f" "$OUT/i386-windows/";   done
for f in "$D3D9_CORE"/build.w64/src/*/unix/*.so;     do copy_if "$f" "$OUT/x86_64-unix/";    done

echo
echo "==> done"
ls -1 "$OUT"/x86_64-windows "$OUT"/i386-windows "$OUT"/x86_64-unix 2>/dev/null | sed 's/^/    /'
