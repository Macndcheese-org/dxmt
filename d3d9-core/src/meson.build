# Trimmed for the d3d9-only build.
#
# This tree exists solely to produce d3d9.dll and its own winemetal9 bridge from
# the GameSir DXMT core.  Everything else -- dxgi, d3d10, d3d11, d3d12, nvapi,
# nvngx, winemetal4 -- is deliberately absent: those come from MacNdCheese's own
# (3Shain-based) dxmt tree and must not be built from this core.  d3d9 does not
# depend on any of them; its dependencies are util, airconv, winemetal and dxmt.

subdir('util')
subdir('airconv')

winemetal_runtime_targets = []
d3d9_postprocess_targets = []

if not dxmt_crossbuild
  error('the d3d9-only tree is a cross build; configure with build-win64.txt or build-win32.txt')
endif

build_dxmt12 = false

subdir('winemetal')
subdir('dxmt')
subdir('d3d9')

dxmt_d3d9_runtime_targets = [ d3d9_dll ]
dxmt_d3d9_runtime_targets += winemetal_runtime_targets
dxmt_d3d9_runtime_targets += d3d9_postprocess_targets

alias_target('dxmt-d3d9', dxmt_d3d9_runtime_targets)
