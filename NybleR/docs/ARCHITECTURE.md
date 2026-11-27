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

Dawn, SDL3 GPU, and raylib are not the public API. They are possible future
implementation details behind this surface.

## Shader strategy

NybleR accepts full WGSL source through Tint. The preferred binary IR is SPIR-V.

```text
WGSL → Tint → SPIR-V → Vulkan/lavapipe
WGSL → Tint/GLSL → OpenGL/llvmpipe
SPIR-V input → validate → Vulkan/lavapipe
```

GLSL/MSL/HLSL writers are kept for backend portability. Mesa only consumes the
GLSL and SPIR-V paths.

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
