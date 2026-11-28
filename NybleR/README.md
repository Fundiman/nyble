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
NybleR painter + Trex glyph atlas (FreeType + HarfBuzz)
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
| `third_party/trex` | Trex | Glyph atlas and HarfBuzz text shaping |
| `third_party/freetype` | FreeType | Font rasterization for Trex |
| `third_party/harfbuzz` | HarfBuzz | Text shaping for Trex |
| `third_party/stb` | stb | Trex image utilities |

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
- `trex` — build and install Trex, FreeType, and HarfBuzz from local vendored sources
- `deps` — all five local dependency builds
- `nybler` — build the NybleR static library and C/C++ headers
- `dom-smoke` — public `nybler_*` DOM API smoke test: `<h1> hello world </h1>` → `hello world`
- `layout-smoke` — inline style, block boxes, text runs, and display-list smoke test
- `shader-smoke` — instance/adapter/device lifecycle and full WGSL → SPIR-V/GLSL compile smoke test
- `resources-smoke` — buffers, textures, samplers, bind groups, pipelines, render passes, command buffers, and queue submission smoke test
- `mesa-smoke` — local Mesa clear, triangle draw, texture-to-buffer copy, and pixel readback smoke test
- `paint-smoke` — DOM → layout → display list → colored quads → GPU readback smoke test
- `text-smoke` — DOM → layout → display list → Trex glyph atlas → GPU readback smoke test
- `check` — run all seven smoke tests
- `all` — `deps` plus `nybler`
- `clean` — remove local build outputs

The Tint sync uses `config/tint.gclient`, a minimal checkout that skips Chromium
test infrastructure, CTS, DXC, libc++, protobuf, googletest, benchmark, and
Vulkan validation layers. CIPD dependencies are ignored because CMake uses the
local Ninja/CMake installation. `NYBLER_JOBS` defaults to `2`; raise or lower it
with, for example, `make -C NybleR tint NYBLER_JOBS=1`.

The default Windows Mesa configuration uses `softpipe` with Vulkan disabled
until a local `llvm-config` is available; llvmpipe and lavapipe remain the
intended performance paths. Mesa is compiled with MinGW GCC on Windows because
clang 22 exhausts the paging file while parsing Mesa's generated `api_trace.c`,
even at `-O0`. NybleR and Tint still use clang. `MESA_CC`, `MESA_CXX`,
`MESA_TARGET_FLAGS`, `MESA_BUILDTYPE`, and `MESA_ARGS` can be overridden to
enable LLVM drivers or a different toolchain when available.

## Backend behavior

`nybler_init()` accepts `NYBLER_BACKEND_AUTO`, `NYBLER_BACKEND_MESA_GL`, and
`NYBLER_BACKEND_NULL`. Mesa Vulkan remains unavailable locally because lavapipe
requires LLVM. `AUTO` uses Mesa GL when EGL/GLES initialization succeeds and
falls back to the null backend otherwise; explicit Mesa GL fails with
`NYBLER_ERROR_BACKEND_UNAVAILABLE`.

The Mesa backend resolves `libEGL`/`libGLESv2` entry points at runtime, so the
stable `nybler_*` ABI does not link against a Mesa build. Consumers must make
the locally built DLLs in `NybleR/build/local/bin` discoverable (for example via
`PATH` on Windows). `make -C NybleR check` sets this path for every smoke test.

`nybler_command_encoder_copy_texture_to_buffer()` records a WebGPU-shaped
texture readback into a `NYBLER_BUFFER_USAGE_COPY_DST` buffer. `bytes_per_row`
must be at least the packed row size; rows are packed into the destination
without a 256-byte alignment requirement. `nybler_buffer_read()` then exposes
the result. Backend-specific format or usage rejections are normal WebGPU
capability behavior.

`nybler_painter_create()` and `nybler_painter_record()` bridge layout to GPU
commands. A painter owns full-WGSL shaders, a render pipeline, and vertex
buffers; recording walks a display list and turns visible `BOX` rects into
clip-space quads with per-vertex colors. Transparent items are skipped. The
painter begins and ends the render pass, so callers finish and submit the
encoder themselves. Release the painter only after submitted commands are no
longer in use.

`nybler_text_renderer_create()` builds a real glyph pipeline from a font file
using Trex (FreeType rasterization and HarfBuzz shaping). It owns an RGBA glyph
atlas texture, a nearest-filter sampler, a bind group, full-WGSL text shaders,
and a textured render pipeline. Attach it to a painter with
`nybler_painter_set_text_renderer()`; the painter borrows the renderer, which
must stay alive while commands are in use. After attachment, visible `TEXT`
display items are shaped into glyph quads with UVs and drawn in the same render
pass as colored boxes. `NyblerTextureDescriptor.initial_data` supplies the
atlas upload. The Mesa backend binds the atlas through a functional bind group
and samples coverage in the textured GLSL variant; both the colored and textured
shader modules are still fully validated as WGSL and compiled to SPIR-V by Tint.

`depot_tools`, CMake, Ninja, Meson, Python, and an LLVM-capable toolchain are
build tools. No Rust bootstrap, MSVC, BoringSSL, or WSL is required. NybleR uses
clang with C++20 for its own sources and Trex bridge; Mesa on Windows uses MinGW
GCC as described above.
