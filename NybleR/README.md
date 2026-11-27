# NybleR

NybleR is the native C/C++ renderer component of Nyble. The `R` means
**Renderer**, not Rust.

NybleR exposes a WebGPU-style API to NybleJS while keeping the actual graphics
implementation private. The first implementation path is:

```text
Lexbor DOM/CSSOM
    ↓
NybleR style + layout + display list
    ↓
NybleR WebGPU-style native API
    ↓
Tint: full WGSL → SPIR-V / GLSL / MSL / HLSL
    ↓
Mesa: EGL/OpenGL llvmpipe or Vulkan lavapipe
```

## Local source policy

NybleR does **not** assume a system Mesa install. Rendering dependencies are
built locally from source submodules:

| Path | Project | Role |
| --- | --- | --- |
| `third_party/lexbor` | Lexbor | HTML5, DOM, CSS, selectors, URL, encoding |
| `third_party/tint` | Tint | Full WGSL parsing/validation and SPIR-V/GLSL/MSL output |
| `third_party/tint/.../glslang` | glslang | Local `glslangValidator` required by Mesa's GLSL build |
| `third_party/mesa` | Mesa | OpenGL/EGL and Vulkan software/hardware rendering |

On Windows, Meson also needs Flex and Bison. NybleR downloads pinned
`winflexbison` tools into `NybleR/build/local/bin`, including the `data/` M4
macros next to `win_bison.exe`; no system MSYS2 installation is used.
`tools/meson_clock.py` wraps Meson locally so Windows file timestamps that are a
fraction of a second in the future do not abort configuration.

The Tint checkout is the pinned, read-only Tint mirror. That upstream repository
states that Tint development now happens in Dawn, so this source is treated as a
pinned vendor snapshot; future refreshes can be taken from `dawn/src/tint`.

## Build

Each component SHOULD have a Makefile. From the repository root:

```sh
make -C NybleR deps
make -C NybleR
```

Targets:

- `lexbor` — build and install local Lexbor into `NybleR/build/local`
- `tint` — fetch Tint's pinned build dependencies with `gclient`, then build Tint
- `glslang` — build Tint's vendored glslang and install `glslangValidator`
- `mesa` — build local Mesa with EGL, GLES2, and software GL drivers
- `deps` — all four local dependency builds
- `nybler` — build the NybleR static library and C/C++ headers
- `dom-smoke` — public `nybler_*` DOM API smoke test: `<h1> hello world </h1>` → `hello world`
- `layout-smoke` — inline style, block boxes, text runs, and display-list smoke test
- `shader-smoke` — instance/adapter/device lifecycle and full WGSL → SPIR-V/GLSL compile smoke test
- `resources-smoke` — buffers, textures, samplers, bind groups, pipelines, render passes, command buffers, and queue submission smoke test
- `check` — run all four smoke tests
- `all` — `deps` plus `nybler`
- `clean` — remove local build outputs

The Tint sync uses `config/tint.gclient`, a minimal checkout that skips Chromium
test infrastructure, CTS, DXC, libc++, protobuf, googletest, benchmark, and
Vulkan validation layers. CIPD dependencies are ignored because CMake uses the
local Ninja/CMake installation. `NYBLER_JOBS` defaults to `2`; raise or lower it
with, for example, `make -C NybleR tint NYBLER_JOBS=1`.

The default Windows Mesa configuration uses `softpipe` with Vulkan disabled
until a local `llvm-config` is available; llvmpipe and lavapipe remain the
intended performance paths. It uses Meson's `plain` build type because the
`release` preset overwrites `-Doptimization` with `-O3`, and Mesa's generated
`api_trace.c` makes clang exhaust memory at that level. `MESA_BUILDTYPE` and
`MESA_ARGS` can be overridden to enable LLVM drivers or a different
optimization level when the local toolchain has enough memory.

`depot_tools`, CMake, Ninja, Meson, Python, and an LLVM-capable toolchain are
build tools. No Rust bootstrap, MSVC, BoringSSL, or WSL is required.
