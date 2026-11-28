#include "internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct NyblerPainterBuffer {
  NyblerBuffer *buffer;
  struct NyblerPainterBuffer *next;
} NyblerPainterBuffer;

struct NyblerPainter {
  NyblerDevice *device;
  NyblerShaderModule *vertex_module;
  NyblerShaderModule *fragment_module;
  NyblerRenderPipeline *pipeline;
  NyblerTextRenderer *text_renderer;
  NyblerPainterBuffer *buffers;
};

static const char painter_vertex_wgsl[] =
  "struct VertexOut {\n"
  "  @builtin(position) position : vec4<f32>,\n"
  "  @location(0) color : vec4<f32>,\n"
  "};\n"
  "@vertex\n"
  "fn vs_main(\n"
  "  @location(0) position : vec2<f32>,\n"
  "  @location(1) color : vec4<f32>\n"
  ") -> VertexOut {\n"
  "  var out : VertexOut;\n"
  "  out.position = vec4<f32>(position, 0.0, 1.0);\n"
  "  out.color = color;\n"
  "  return out;\n"
  "}\n";

static const char painter_fragment_wgsl[] =
  "@fragment\n"
  "fn fs_main(@location(0) color : vec4<f32>) -> @location(0) vec4<f32> {\n"
  "  return color;\n"
  "}\n";

static void nybler_painter_write_vertex(
  float *vertices,
  size_t *index,
  float x,
  float y,
  const NyblerColor *color
) {
  size_t base = *index * 6;
  vertices[base] = x;
  vertices[base + 1] = y;
  vertices[base + 2] = color->r;
  vertices[base + 3] = color->g;
  vertices[base + 4] = color->b;
  vertices[base + 5] = color->a;
  (*index)++;
}

static void nybler_painter_write_quad(
  float *vertices,
  size_t *index,
  const NyblerViewport *viewport,
  const NyblerRect *rect,
  const NyblerColor *color
) {
  float left = (rect->x / viewport->width) * 2.0f - 1.0f;
  float right =
    ((rect->x + rect->width) / viewport->width) * 2.0f - 1.0f;
  float top = 1.0f - (rect->y / viewport->height) * 2.0f;
  float bottom =
    1.0f - ((rect->y + rect->height) / viewport->height) * 2.0f;

  nybler_painter_write_vertex(vertices, index, left, top, color);
  nybler_painter_write_vertex(vertices, index, left, bottom, color);
  nybler_painter_write_vertex(vertices, index, right, top, color);
  nybler_painter_write_vertex(vertices, index, right, top, color);
  nybler_painter_write_vertex(vertices, index, left, bottom, color);
  nybler_painter_write_vertex(vertices, index, right, bottom, color);
}

static int nybler_painter_item_visible(const NyblerDisplayItem *item) {
  return item &&
    (item->kind == NYBLER_DISPLAY_ITEM_BOX ||
     item->kind == NYBLER_DISPLAY_ITEM_TEXT) &&
    item->rect.width > 0.0f && item->rect.height > 0.0f &&
    item->color.a > 0.0f;
}

static int nybler_painter_append_buffer(
  NyblerPainter *painter,
  NyblerBuffer *buffer
) {
  NyblerPainterBuffer *entry =
    (NyblerPainterBuffer *)calloc(1, sizeof(*entry));

  if (!entry) return 0;
  entry->buffer = buffer;
  entry->next = painter->buffers;
  painter->buffers = entry;
  return 1;
}

NyblerResult nybler_painter_create(
  NyblerDevice *device,
  NyblerPainter **out_painter
) {
  static const NyblerVertexAttribute attributes[] = {
    {0, 0, NYBLER_VERTEX_FORMAT_FLOAT32X2},
    {1, 8, NYBLER_VERTEX_FORMAT_FLOAT32X4}
  };
  static const NyblerColorTargetState target = {
    NYBLER_TEXTURE_FORMAT_RGBA8_UNORM, 0
  };
  NyblerRenderPipelineDescriptor pipeline_descriptor;
  NyblerPainter *painter;
  NyblerResult result;

  if (!device || !out_painter) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_painter = NULL;

  painter = (NyblerPainter *)calloc(1, sizeof(*painter));
  if (!painter) return NYBLER_ERROR_OUT_OF_MEMORY;
  painter->device = device;

  result = nybler_shader_module_create(
    device,
    NYBLER_SHADER_FORMAT_WGSL,
    painter_vertex_wgsl,
    sizeof(painter_vertex_wgsl) - 1,
    "nybler painter vertex",
    &painter->vertex_module
  );
  if (result != NYBLER_OK) goto fail;
  result = nybler_shader_module_create(
    device,
    NYBLER_SHADER_FORMAT_WGSL,
    painter_fragment_wgsl,
    sizeof(painter_fragment_wgsl) - 1,
    "nybler painter fragment",
    &painter->fragment_module
  );
  if (result != NYBLER_OK) goto fail;

  memset(&pipeline_descriptor, 0, sizeof(pipeline_descriptor));
  pipeline_descriptor.vertex_module = painter->vertex_module;
  pipeline_descriptor.fragment_module = painter->fragment_module;
  pipeline_descriptor.vertex_entry_point = "vs_main";
  pipeline_descriptor.fragment_entry_point = "fs_main";
  pipeline_descriptor.targets = &target;
  pipeline_descriptor.target_count = 1;
  pipeline_descriptor.topology = NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_descriptor.vertex_attributes = attributes;
  pipeline_descriptor.vertex_attribute_count = 2;
  pipeline_descriptor.vertex_stride = 24;
  pipeline_descriptor.label = "nybler painter";
  result = nybler_device_create_render_pipeline(
    device, &pipeline_descriptor, &painter->pipeline);
  if (result != NYBLER_OK) goto fail;

  *out_painter = painter;
  return NYBLER_OK;

fail:
  nybler_painter_release(painter);
  return result;
}

void nybler_painter_set_text_renderer(
  NyblerPainter *painter,
  NyblerTextRenderer *renderer
) {
  if (painter) painter->text_renderer = renderer;
}

void nybler_painter_release(NyblerPainter *painter) {
  NyblerPainterBuffer *entry;
  NyblerPainterBuffer *next;

  if (!painter) return;
  entry = painter->buffers;
  while (entry) {
    next = entry->next;
    nybler_buffer_release(entry->buffer);
    free(entry);
    entry = next;
  }
  nybler_render_pipeline_release(painter->pipeline);
  nybler_shader_module_release(painter->fragment_module);
  nybler_shader_module_release(painter->vertex_module);
  free(painter);
}

NyblerResult nybler_painter_record(
  NyblerPainter *painter,
  NyblerCommandEncoder *encoder,
  const NyblerViewport *viewport,
  const NyblerDisplayList *display_list,
  const NyblerRenderPassDescriptor *render_pass
) {
  NyblerRenderPassEncoder *pass = NULL;
  NyblerBuffer *color_vertices = NULL;
  NyblerBuffer *text_vertices = NULL;
  const NyblerRenderPassColorAttachment *attachment;
  float *color_quads = NULL;
  float *text_quads = NULL;
  size_t color_count = 0;
  size_t text_count = 0;
  size_t text_glyph_capacity = 0;
  size_t color_index = 0;
  size_t text_index = 0;
  size_t i;
  NyblerResult result;
  NyblerBufferDescriptor descriptor;

  if (!painter || !encoder || !viewport || !display_list || !render_pass) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (viewport->width <= 0.0f || viewport->height <= 0.0f ||
      render_pass->color_attachment_count != 1 ||
      !render_pass->color_attachments ||
      !render_pass->color_attachments[0].texture) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  attachment = &render_pass->color_attachments[0];
  (void)attachment;

  for (i = 0; i < nybler_display_list_length(display_list); ++i) {
    const NyblerDisplayItem *item = nybler_display_list_item(display_list, i);
    if (!nybler_painter_item_visible(item)) continue;
    if (painter->text_renderer && item->kind == NYBLER_DISPLAY_ITEM_TEXT) {
      text_count++;
      text_glyph_capacity += item->text_length;
    } else {
      color_count++;
    }
  }
  if (color_count > (SIZE_MAX / 6) / sizeof(float) ||
      text_glyph_capacity > SIZE_MAX / (6 * 8 * sizeof(float))) {
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  if (color_count) {
    color_quads = (float *)calloc(
      color_count * 6 * 6, sizeof(*color_quads));
    if (!color_quads) return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  if (text_glyph_capacity) {
    text_quads = (float *)calloc(
      text_glyph_capacity * 6 * 8, sizeof(*text_quads));
    if (!text_quads) {
      free(color_quads);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }

  for (i = 0; i < nybler_display_list_length(display_list); ++i) {
    const NyblerDisplayItem *item = nybler_display_list_item(display_list, i);
    if (!nybler_painter_item_visible(item)) continue;
    if (painter->text_renderer && item->kind == NYBLER_DISPLAY_ITEM_TEXT) {
      size_t produced = 0;
      if (!nybler_text_shape(
            painter->text_renderer,
            item->text, item->text_length,
            &item->rect, viewport, &item->color,
            text_quads + text_index * 8, &produced,
            (text_glyph_capacity * 6) - text_index)) {
        free(color_quads);
        free(text_quads);
        return NYBLER_ERROR_UNSUPPORTED;
      }
      text_index += produced;
    } else {
      nybler_painter_write_quad(
        color_quads, &color_index, viewport, &item->rect, &item->color);
    }
  }

  if (color_count) {
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.size = (uint64_t)color_count * 6 * 6 * sizeof(float);
    descriptor.usage = NYBLER_BUFFER_USAGE_VERTEX;
    descriptor.initial_data = color_quads;
    descriptor.label = "nybler painter color vertices";
    result = nybler_device_create_buffer(
      painter->device, &descriptor, &color_vertices);
    if (result != NYBLER_OK) goto fail_buffers;
    if (!nybler_painter_append_buffer(painter, color_vertices)) {
      nybler_buffer_release(color_vertices);
      color_vertices = NULL;
      result = NYBLER_ERROR_OUT_OF_MEMORY;
      goto fail_buffers;
    }
  }
  if (text_count && text_index) {
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.size = (uint64_t)text_index * 8 * sizeof(float);
    descriptor.usage = NYBLER_BUFFER_USAGE_VERTEX;
    descriptor.initial_data = text_quads;
    descriptor.label = "nybler painter text vertices";
    result = nybler_device_create_buffer(
      painter->device, &descriptor, &text_vertices);
    if (result != NYBLER_OK) goto fail_buffers;
    if (!nybler_painter_append_buffer(painter, text_vertices)) {
      nybler_buffer_release(text_vertices);
      text_vertices = NULL;
      result = NYBLER_ERROR_OUT_OF_MEMORY;
      goto fail_buffers;
    }
  }

  free(color_quads);
  free(text_quads);
  color_quads = NULL;
  text_quads = NULL;

  result = nybler_command_encoder_begin_render_pass(
    encoder, render_pass, &pass);
  if (result != NYBLER_OK) return result;
  if (color_vertices) {
    nybler_render_pass_set_pipeline(pass, painter->pipeline);
    nybler_render_pass_set_vertex_buffer(pass, 0, color_vertices, 0);
    nybler_render_pass_draw(pass, (uint32_t)color_count * 6, 1, 0, 0);
  }
  if (text_vertices && painter->text_renderer) {
    nybler_render_pass_set_pipeline(
      pass, painter->text_renderer->pipeline);
    nybler_render_pass_set_bind_group(
      pass, 0, painter->text_renderer->bind_group);
    nybler_render_pass_set_vertex_buffer(pass, 0, text_vertices, 0);
    nybler_render_pass_draw(pass, (uint32_t)text_index, 1, 0, 0);
  }
  nybler_render_pass_end(pass);
  return NYBLER_OK;

fail_buffers:
  free(color_quads);
  free(text_quads);
  return result;
}
