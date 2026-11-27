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
| `third_party/tint` | Tint | Full WGSL parsing/validation and SPIR-V/GLSL/MSL/HLSL output |
| `third_party/mesa` | Mesa | OpenGL/EGL and Vulkan software/hardware rendering |

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
- `tint` — fetch Tint's pinned dependencies with `gclient`, then build Tint
- `mesa` — build local Mesa with software GL/Vulkan drivers
- `deps` — all three local dependency builds
- `nybler` — build the NybleR static library and C/C++ headers
- `dom-smoke` — locally built Lexbor smoke test: `<h1> hello world </h1>` → `hello world`
- `all` — `deps` plus `nybler`
- `clean` — remove local build outputs

`depot_tools`, CMake, Ninja, Meson, Python, and an LLVM-capable toolchain are
build tools. No Rust bootstrap, MSVC, BoringSSL, or WSL is required.
