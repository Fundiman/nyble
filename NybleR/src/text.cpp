#include "internal.h"

#include <Trex/Atlas.hpp>
#include <Trex/Charset.hpp>
#include <Trex/TextShaper.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

static const char text_vertex_wgsl[] =
  "struct VertexOut {\n"
  "  @builtin(position) position : vec4<f32>,\n"
  "  @location(0) uv : vec2<f32>,\n"
  "  @location(1) color : vec4<f32>,\n"
  "};\n"
  "@vertex\n"
  "fn vs_main(\n"
  "  @location(0) position : vec2<f32>,\n"
  "  @location(1) uv : vec2<f32>,\n"
  "  @location(2) color : vec4<f32>\n"
  ") -> VertexOut {\n"
  "  var out : VertexOut;\n"
  "  out.position = vec4<f32>(position, 0.0, 1.0);\n"
  "  out.uv = uv;\n"
  "  out.color = color;\n"
  "  return out;\n"
  "}\n";

static const char text_fragment_wgsl[] =
  "@group(0) @binding(0) var atlas_texture : texture_2d<f32>;\n"
  "@group(0) @binding(1) var atlas_sampler : sampler;\n"
  "@fragment\n"
  "fn fs_main(\n"
  "  @location(0) uv : vec2<f32>,\n"
  "  @location(1) color : vec4<f32>\n"
  ") -> @location(0) vec4<f32> {\n"
  "  let coverage = textureSample(atlas_texture, atlas_sampler, uv).a;\n"
  "  return vec4<f32>(color.rgb, color.a * coverage);\n"
  "}\n";

static void nybler_text_release_resources(NyblerTextRenderer *renderer) {
  nybler_render_pipeline_release(renderer->pipeline);
  nybler_shader_module_release(renderer->fragment_module);
  nybler_shader_module_release(renderer->vertex_module);
  nybler_bind_group_release(renderer->bind_group);
  nybler_bind_group_layout_release(renderer->bind_group_layout);
  nybler_sampler_release(renderer->sampler);
  nybler_texture_release(renderer->atlas_texture);
  delete reinterpret_cast<Trex::TextShaper *>(renderer->shaper);
  delete reinterpret_cast<Trex::Atlas *>(renderer->atlas);
  renderer->pipeline = NULL;
  renderer->fragment_module = NULL;
  renderer->vertex_module = NULL;
  renderer->bind_group = NULL;
  renderer->bind_group_layout = NULL;
  renderer->sampler = NULL;
  renderer->atlas_texture = NULL;
  renderer->shaper = NULL;
  renderer->atlas = NULL;
}

NyblerResult nybler_text_renderer_create(
  NyblerDevice *device,
  const char *font_path,
  float font_size,
  NyblerTextRenderer **out_renderer
) {
  static const NyblerVertexAttribute attributes[] = {
    {0, 0,  NYBLER_VERTEX_FORMAT_FLOAT32X2},
    {1, 8,  NYBLER_VERTEX_FORMAT_FLOAT32X2},
    {2, 16, NYBLER_VERTEX_FORMAT_FLOAT32X4}
  };
  static const NyblerColorTargetState target = {
    NYBLER_TEXTURE_FORMAT_RGBA8_UNORM, 1
  };
  NyblerTextRenderer *renderer;
  Trex::Atlas *atlas = NULL;
  Trex::TextShaper *shaper = NULL;
  const Trex::Atlas::Bitmap *bitmap;
  std::vector<uint8_t> rgba;
  NyblerTextureDescriptor texture_descriptor;
  NyblerSamplerDescriptor sampler_descriptor;
  NyblerBindGroupLayoutEntry layout_entries[2];
  NyblerBindGroupLayoutDescriptor layout_descriptor;
  NyblerBindGroupEntry bind_entries[2];
  NyblerBindGroupDescriptor bind_descriptor;
  NyblerRenderPipelineDescriptor pipeline_descriptor;
  NyblerResult result;

  if (!device || !font_path || !out_renderer || font_size <= 0.0f) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  *out_renderer = NULL;

  renderer = new NyblerTextRenderer();
  if (!renderer) return NYBLER_ERROR_OUT_OF_MEMORY;
  memset(renderer, 0, sizeof(*renderer));
  renderer->device = device;
  renderer->font_size = font_size;

  try {
    int size = (int)font_size;
    if (size < 1) size = 1;
    atlas = new Trex::Atlas(font_path, size, Trex::Charset::Ascii());
    shaper = new Trex::TextShaper(*atlas);
  } catch (...) {
    delete shaper;
    delete atlas;
    delete renderer;
    return NYBLER_ERROR_UNSUPPORTED;
  }
  renderer->atlas = atlas;
  renderer->shaper = shaper;

  bitmap = &atlas->GetBitmap();
  renderer->atlas_width = bitmap->Width();
  renderer->atlas_height = bitmap->Height();
  if (renderer->atlas_width == 0 || renderer->atlas_height == 0) {
    nybler_text_release_resources(renderer);
    delete renderer;
    return NYBLER_ERROR_UNSUPPORTED;
  }

  rgba.resize((size_t)renderer->atlas_width * renderer->atlas_height * 4);
  for (unsigned int i = 0; i < renderer->atlas_width * renderer->atlas_height; ++i) {
    uint8_t coverage = bitmap->Data()[i * bitmap->Channels()];
    rgba[i * 4 + 0] = 255;
    rgba[i * 4 + 1] = 255;
    rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = coverage;
  }

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size.width = renderer->atlas_width;
  texture_descriptor.size.height = renderer->atlas_height;
  texture_descriptor.size.depth_or_array_layers = 1;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage =
    NYBLER_TEXTURE_USAGE_TEXTURE_BINDING | NYBLER_TEXTURE_USAGE_COPY_DST;
  texture_descriptor.initial_data = rgba.data();
  texture_descriptor.label = "nybler glyph atlas";
  result = nybler_device_create_texture(
    device, &texture_descriptor, &renderer->atlas_texture);
  if (result != NYBLER_OK) goto fail;

  memset(&sampler_descriptor, 0, sizeof(sampler_descriptor));
  sampler_descriptor.address_mode_u = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.address_mode_v = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.address_mode_w = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.min_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.mag_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.mipmap_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.compare = NYBLER_COMPARE_FUNCTION_NONE;
  sampler_descriptor.label = "nybler glyph sampler";
  result = nybler_device_create_sampler(
    device, &sampler_descriptor, &renderer->sampler);
  if (result != NYBLER_OK) goto fail;

  memset(layout_entries, 0, sizeof(layout_entries));
  layout_entries[0].binding = 0;
  layout_entries[0].visibility = NYBLER_SHADER_STAGE_FRAGMENT;
  layout_entries[0].binding_type = NYBLER_BINDING_TYPE_TEXTURE;
  layout_entries[1].binding = 1;
  layout_entries[1].visibility = NYBLER_SHADER_STAGE_FRAGMENT;
  layout_entries[1].binding_type = NYBLER_BINDING_TYPE_SAMPLER;
  memset(&layout_descriptor, 0, sizeof(layout_descriptor));
  layout_descriptor.entries = layout_entries;
  layout_descriptor.entry_count = 2;
  layout_descriptor.label = "nybler glyph layout";
  result = nybler_device_create_bind_group_layout(
    device, &layout_descriptor, &renderer->bind_group_layout);
  if (result != NYBLER_OK) goto fail;

  memset(bind_entries, 0, sizeof(bind_entries));
  bind_entries[0].binding = 0;
  bind_entries[0].texture = renderer->atlas_texture;
  bind_entries[1].binding = 1;
  bind_entries[1].sampler = renderer->sampler;
  memset(&bind_descriptor, 0, sizeof(bind_descriptor));
  bind_descriptor.layout = renderer->bind_group_layout;
  bind_descriptor.entries = bind_entries;
  bind_descriptor.entry_count = 2;
  bind_descriptor.label = "nybler glyph bind group";
  result = nybler_device_create_bind_group(
    device, &bind_descriptor, &renderer->bind_group);
  if (result != NYBLER_OK) goto fail;

  result = nybler_shader_module_create(
    device, NYBLER_SHADER_FORMAT_WGSL,
    text_vertex_wgsl, sizeof(text_vertex_wgsl) - 1,
    "nybler text vertex", &renderer->vertex_module);
  if (result != NYBLER_OK) goto fail;
  result = nybler_shader_module_create(
    device, NYBLER_SHADER_FORMAT_WGSL,
    text_fragment_wgsl, sizeof(text_fragment_wgsl) - 1,
    "nybler text fragment", &renderer->fragment_module);
  if (result != NYBLER_OK) goto fail;

  memset(&pipeline_descriptor, 0, sizeof(pipeline_descriptor));
  pipeline_descriptor.vertex_module = renderer->vertex_module;
  pipeline_descriptor.fragment_module = renderer->fragment_module;
  pipeline_descriptor.vertex_entry_point = "vs_main";
  pipeline_descriptor.fragment_entry_point = "fs_main";
  pipeline_descriptor.targets = &target;
  pipeline_descriptor.target_count = 1;
  pipeline_descriptor.topology = NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_descriptor.layout = renderer->bind_group_layout;
  pipeline_descriptor.vertex_attributes = attributes;
  pipeline_descriptor.vertex_attribute_count = 3;
  pipeline_descriptor.vertex_stride = 32;
  pipeline_descriptor.label = "nybler text";
  result = nybler_device_create_render_pipeline(
    device, &pipeline_descriptor, &renderer->pipeline);
  if (result != NYBLER_OK) goto fail;

  *out_renderer = renderer;
  return NYBLER_OK;

fail:
  nybler_text_release_resources(renderer);
  delete renderer;
  return result;
}

void nybler_text_renderer_release(NyblerTextRenderer *renderer) {
  if (!renderer) return;
  nybler_text_release_resources(renderer);
  delete renderer;
}

int nybler_text_shape(
  NyblerTextRenderer *renderer,
  const char *text,
  size_t text_length,
  const NyblerRect *rect,
  const NyblerViewport *viewport,
  const NyblerColor *color,
  float *vertices,
  size_t *vertex_count,
  size_t capacity
) {
  Trex::TextShaper *shaper;
  Trex::ShapedGlyphs shaped;
  Trex::TextMeasurement measurement;
  float baseline_x;
  float baseline_y;
  float cursor_x = 0.0f;
  float cursor_y = 0.0f;
  size_t count = 0;
  size_t i;

  if (!renderer || !text || !rect || !viewport || !color ||
      !vertices || !vertex_count || viewport->width <= 0.0f ||
      viewport->height <= 0.0f) {
    return 0;
  }
  shaper = reinterpret_cast<Trex::TextShaper *>(renderer->shaper);
  if (!shaper) return 0;
  *vertex_count = 0;

  try {
    shaped = shaper->ShapeUtf8(std::span<const char>(text, text_length));
  } catch (...) {
    return 0;
  }
  if (shaped.empty()) return 1;

  measurement = Trex::TextShaper::Measure(shaped);
  baseline_x = rect->x - measurement.xOffset;
  baseline_y = rect->y - measurement.yOffset;

  for (i = 0; i < shaped.size(); ++i) {
    const Trex::ShapedGlyph &glyph = shaped[i];
    float gx = cursor_x + glyph.xOffset + (float)glyph.info.bearingX;
    float gy = cursor_y + glyph.yOffset - (float)glyph.info.bearingY;
    float left, top, right, bottom;
    float u0, v0, u1, v1;
    float quad[6][8];

    cursor_x += glyph.xAdvance;
    cursor_y += glyph.yAdvance;
    if (glyph.info.width == 0 || glyph.info.height == 0) continue;
    if (count + 6 > capacity) return 0;

    left = baseline_x + gx;
    top = baseline_y + gy;
    right = left + (float)glyph.info.width;
    bottom = top + (float)glyph.info.height;

    u0 = (float)glyph.info.x / (float)renderer->atlas_width;
    v0 = (float)glyph.info.y / (float)renderer->atlas_height;
    u1 = (float)(glyph.info.x + glyph.info.width) /
      (float)renderer->atlas_width;
    v1 = (float)(glyph.info.y + glyph.info.height) /
      (float)renderer->atlas_height;

    quad[0][0] = (left / viewport->width) * 2.0f - 1.0f;
    quad[0][1] = 1.0f - (top / viewport->height) * 2.0f;
    quad[1][0] = quad[0][0];
    quad[1][1] = 1.0f - (bottom / viewport->height) * 2.0f;
    quad[2][0] = (right / viewport->width) * 2.0f - 1.0f;
    quad[2][1] = quad[0][1];
    quad[3][0] = quad[2][0];
    quad[3][1] = quad[0][1];
    quad[4][0] = quad[0][0];
    quad[4][1] = quad[1][1];
    quad[5][0] = quad[2][0];
    quad[5][1] = quad[1][1];

    for (size_t v = 0; v < 6; ++v) {
      quad[v][2] = (v == 0 || v == 1 || v == 4) ? u0 : u1;
      quad[v][3] = (v == 0 || v == 2 || v == 3) ? v0 : v1;
      quad[v][4] = color->r;
      quad[v][5] = color->g;
      quad[v][6] = color->b;
      quad[v][7] = color->a;
    }
    memcpy(vertices + count * 8, quad, sizeof(quad));
    count += 6;
  }

  *vertex_count = count;
  return 1;
}
