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
NybleR painter: colored quads + Trex glyph atlas
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
- programmatic display lists with rect and text items
- instance/adapter/device
- buffers, textures, samplers
- shader modules
- bind group layouts and bind groups
- render/compute pipelines with explicit vertex attributes
- NybleR NyCanvas over NyPicture display lists
- Trex-backed NyFont (glyph atlas, sampler, bind group, text pipeline)
- command encoders and pass encoders
- texture-to-buffer copy views and buffer readback
- queue submission and texture readback (no windowed present path yet)

`nybler.hpp` stays a thin, header-only layer over that C ABI. GPU objects keep
WebGPU names (`Buffer`, `Texture`, `CommandEncoder`, `CommandBuffer`); painting
uses NybleR's Ny-prefixed names (`NyCanvas`, `NyPicture`, `NyFont`, `NyPaint`)
rather than verbatim Skia names, plus `NyColor`, `NyRect`, `NyViewport`,
`NyExtent3D`, `NyOrigin3D`, and `NyPictureItem` value aliases. Owned wrappers
delete copy, support move, and release through the matching `nybler_*_release()`
function.
`RenderPassEncoder` and `Queue` are borrowed handles; the former ends an open
pass on destruction. The copy/readback helpers build
`NyblerImageCopyTexture`/`NyblerImageCopyBuffer` views from wrapper handles,
and `Queue::submit()` transfers `CommandBuffer` ownership to the C API because
`nybler_queue_submit()` consumes command buffers. `NyPicture::append_rect()`
and `append_text()` build a list through additive `nybler_display_list_*`
entry points; `NyCanvas::draw_picture()` plays it through
`nybler_painter_record()`, and `draw_rect()`/`draw_text()` are one-shot
conveniences over that path. `NyCanvas::set_font()` attaches a Trex-backed
`NyFont`. Existing C API names and behavior are unchanged.

Lexbor stays private behind `src/document.c`; callers never see Lexbor types or
headers in `nybler.h`. Document parsing initializes Lexbor's CSSOM before HTML
parsing, so `<style>` text and `style` attributes are parsed and cascaded by
Lexbor. `src/document.c` exposes only serialized computed values to
`src/layout.c`, which consumes the public DOM API, computes block boxes and text
runs, and produces borrowed display-list items. Display lists own copies of
their text so they can outlive a layout object; layouts borrow their source
document.

`src/paint.c` is the display-list paint bridge. `NyblerPainter` owns the
full-WGSL shaders, pipeline, and vertex buffers used to draw `BOX` items as
clip-space quads with per-vertex colors. With an attached `NyblerTextRenderer`,
`TEXT` items are shaped by `src/text.cpp` into textured glyph quads instead.
The bridge computes NDC from the layout viewport and item rectangles, skips
transparent or empty items, then records colored and textured draws in one
normal render pass through the public WebGPU-shaped API. Callers keep the
painter and its borrowed text renderer alive until submitted command buffers
are done.

`src/text.cpp` is the only file that includes Trex headers. It wraps Trex's
`Atlas` and `TextShaper` (backed by FreeType and HarfBuzz), uploads the RGBA
atlas through `NyblerTextureDescriptor.initial_data`, and creates the sampler,
bind group layout, bind group, WGSL shader modules, and textured render pipeline
used by the painter. Glyph positions and UVs are emitted as 32-byte vertices
(position.xy, uv.xy, color.rgba); codepoints are shaped through HarfBuzz rather
than approximated as colored boxes.

`src/resources.c` implements the WebGPU-style object graph. Devices borrow
adapters, resources and queues borrow devices, render pass encoders borrow their
command encoder, and command buffers are consumed by `nybler_queue_submit()`.
Command buffers record validated render-pass and texture-copy commands;
submission executes them through the selected backend before consuming them.

`src/mesa_backend.c` is the local Mesa GL implementation. It resolves EGL and
GLES entry points through `src/backend_loader.c`, creates an offscreen
pbuffer/surfaceless context, backs WebGPU-style textures with GLES textures and
framebuffer objects, and executes clear, draw, and copy commands. The public ABI
does not link against Mesa, so Dawn/Mesa details stay private.
`NYBLER_BACKEND_AUTO` falls back to the null backend when Mesa cannot initialize;
explicit Mesa GL reports `NYBLER_ERROR_BACKEND_UNAVAILABLE`. Keeping the loader
in its own source also lets POSIX CI compile the exact `dlopen`/`dlsym` path
against a mock shared library.

The Mesa draw path honors bind groups generically. It discovers uniform blocks in
the pipeline's generated GLSL, assigns each block to the WebGPU binding number
recorded by the shader bridge, and uploads the matching bound buffer to that
binding point. Tint names combined texture/sampler uniforms deterministically as
`nybler_sampler_<texture-binding>_<sampler-binding>`, so the backend resolves the
sampled texture by binding number rather than bind-group entry order. No sampler
name is hardcoded in the backend. The colored, textured, and bind-group smoke
programs are translations of their own WGSL modules and entry points, not fixed
validation shaders. GLES 3.0 has no shader storage blocks, so WGSL storage
buffers remain a normal backend capability rejection on this backend.

Dawn, SDL3 GPU, and raylib are not the public API. They are possible future
implementation details behind this surface.

## Shader strategy

NybleR accepts full WGSL source through Tint. The preferred binary IR is SPIR-V.
`src/shader_bridge.cpp` is the only C++ file that includes Tint headers; it
exposes small `extern "C"` entry points used by `src/instance.c` and
`src/resources.c`.

```text
WGSL → Tint → SPIR-V → Vulkan/lavapipe
WGSL → Tint → SPIR-V (validation) + Tint AST → GLSL ES 300 → Mesa GL
SPIR-V input → magic/alignment check → Vulkan/lavapipe
```

`nybler_shader_compile()` exposes standalone compilation for tooling and tests.
Shader modules always store SPIR-V; failures carry Tint diagnostics through the
optional `out_error` parameter. Every WGSL module is still parsed, validated,
and compiled to SPIR-V at creation time, including modules used by Mesa GL.

The Mesa GL path generates pipeline-specific GLSL from the original WGSL source
retained on each module. `nybler_device_create_render_pipeline()` asks Tint to
generate GLSL for that pipeline's exact vertex and fragment entry points and
remaps WGSL bindings to the flat bindings assigned by the pipeline layout.
`nybler_tint_translate_glsl()` uses Tint's AST-based `Generate()` path: this
Tint snapshot's IR GLSL printer is a stub that raises an ICE on `core::ir::Var`,
while the AST path is complete and is what the Tint CLI uses.

Before handing GLSL to Mesa, the bridge runs Tint's `Renamer` transform for GLSL
keywords, canonicalizes inter-stage variable names by `@location`, and drops
explicit locations that GLSL ES 300 does not allow on fragment inputs or vertex
outputs. Vertex input and fragment output locations are preserved. GLES 3.0 also
rejects `layout(binding = N)` on buffers and samplers, so the bridge removes that
qualifier and records the number as a trailing `// nybler_binding N` annotation;
binding is then set through GL binding points. The Mesa backend compiles those
exact strings and discovers blocks and combined samplers from the generated
source. A raw SPIR-V module has no WGSL to translate and is rejected by the Mesa
GL backend as a backend capability error.

Backend capability checks are not a WGSL subset parser. A shader may compile as
full WGSL and still be rejected by a specific backend when it uses features that
the backend cannot express.

## Local dependency builds

All renderer dependencies are built locally from source:

- Lexbor via CMake
- Tint via CMake after pinned `gclient sync`
- glslang from Tint's vendored source via CMake, for `glslangValidator`
- Mesa via Meson/Ninja
- Trex/FreeType/HarfBuzz via CMake with vendored FetchContent sources
- winflexbison on Windows, downloaded into the local prefix for Meson

No system Mesa, system Tint, or system Lexbor is used by the NybleR build. The
current Windows default is Mesa `softpipe` with Vulkan disabled because local
LLVM tools are not available; the backend choice is a build-time detail, not
part of the public API.
