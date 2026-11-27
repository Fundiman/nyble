# NybleR architecture

NybleR is split into a browser-facing front half and a GPU-facing back half.

```text
HTML/CSS bytes
    ↓
Lexbor: DOM + CSSOM + selectors
    ↓
NybleR style resolution
    ↓
NybleR layout box tree
    ↓
NybleR display list
    ↓
NybleR WebGPU-style native API
    ↓
Tint: WGSL → SPIR-V / GLSL
    ↓
Mesa: llvmpipe / lavapipe / hardware driver
```

## Public API

The stable public surface is a C ABI in `include/nybler/nybler.h`, with a small
C++ wrapper in `include/nybler/nybler.hpp`.

The API is WebGPU-shaped for rendering and DOM-shaped for platform work:

- parsed documents and borrowed node handles from HTML bytes
- node traversal, local names, attributes, and text content
- style/layout objects, node rectangles, and display-list items
- instance/adapter/device
- buffers, textures, samplers
- shader modules
- bind group layouts and bind groups
- render/compute pipelines
- command encoders and pass encoders
- queue submission and presentation

Lexbor stays private behind `src/document.c`; callers never see Lexbor types or
headers in `nybler.h`. `src/layout.c` consumes only the public DOM API, computes
inline-style-driven block boxes and text runs, and produces borrowed
display-list items. Display lists own copies of their text so they can outlive a
layout object; layouts borrow their source document.

`src/resources.c` implements the WebGPU-style object graph. Devices borrow
adapters, resources and queues borrow devices, render pass encoders borrow their
command encoder, and command buffers are consumed by `nybler_queue_submit()`.
Command buffers currently record validated render-pass commands; Mesa-backed
execution is wired in as the local software renderer is integrated.

Dawn, SDL3 GPU, and raylib are not the public API. They are possible future
implementation details behind this surface.

## Shader strategy

NybleR accepts full WGSL source through Tint. The preferred binary IR is SPIR-V.
`src/shader_bridge.cpp` is the only C++ file that includes Tint headers; it
exposes a small `extern "C"` entry point used by `src/instance.c`.

```text
WGSL → Tint → SPIR-V → Vulkan/lavapipe
WGSL → Tint/GLSL → OpenGL/llvmpipe
SPIR-V input → magic/alignment check → Vulkan/lavapipe
```

`nybler_shader_compile()` exposes standalone compilation for tooling and tests.
Shader modules always store SPIR-V; failures carry Tint diagnostics through the
optional `out_error` parameter. GLSL output currently targets the `main` entry
point for the local Mesa GL path.

Backend capability checks are not a WGSL subset parser. A shader may compile as
full WGSL and still be rejected by a specific backend when it uses features that
the backend cannot express.

## Local dependency builds

All renderer dependencies are built locally from source:

- Lexbor via CMake
- Tint via CMake after pinned `gclient sync`
- glslang from Tint's vendored source via CMake, for `glslangValidator`
- Mesa via Meson/Ninja
- winflexbison on Windows, downloaded into the local prefix for Meson

No system Mesa, system Tint, or system Lexbor is used by the NybleR build. The
current Windows default is Mesa `softpipe` with Vulkan disabled because local
LLVM tools are not available; the backend choice is a build-time detail, not
part of the public API.
