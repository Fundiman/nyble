#include "internal.h"
#include "mesa_backend.h"
#include "shader_bridge.h"

#include <stdlib.h>
#include <string.h>

char *nybler_internal_duplicate_string(const char *value) {
  char *copy;
  size_t length;

  if (!value) return NULL;
  length = strlen(value);
  copy = (char *)malloc(length + 1);
  if (!copy) return NULL;
  memcpy(copy, value, length + 1);
  return copy;
}

void nybler_internal_free_commands(NyblerCommand *commands, size_t count) {
  size_t i;

  if (!commands) return;
  for (i = 0; i < count; ++i) {
    if (commands[i].kind == NYBLER_COMMAND_BEGIN_RENDER_PASS) {
      free(commands[i].data.render_pass.attachments);
      free(commands[i].data.render_pass.label);
    }
  }
  free(commands);
}

static int nybler_in_range(uint32_t value, uint32_t first, uint32_t last) {
  return value >= first && value <= last;
}

static int nybler_buffer_fits(const NyblerBuffer *buffer, uint64_t offset, uint64_t size) {
  if (!buffer || offset > buffer->size) return 0;
  return size <= buffer->size - offset;
}

static uint32_t nybler_vertex_format_size(NyblerVertexFormat format) {
  switch (format) {
    case NYBLER_VERTEX_FORMAT_FLOAT32: return 4;
    case NYBLER_VERTEX_FORMAT_FLOAT32X2: return 8;
    case NYBLER_VERTEX_FORMAT_FLOAT32X3: return 12;
    case NYBLER_VERTEX_FORMAT_FLOAT32X4: return 16;
    default: return 0;
  }
}

static uint32_t nybler_texture_bytes_per_pixel(NyblerTextureFormat format) {
  switch (format) {
    case NYBLER_TEXTURE_FORMAT_R8_UNORM: return 1;
    case NYBLER_TEXTURE_FORMAT_RG8_UNORM: return 2;
    case NYBLER_TEXTURE_FORMAT_RGBA8_UNORM:
    case NYBLER_TEXTURE_FORMAT_BGRA8_UNORM:
    case NYBLER_TEXTURE_FORMAT_R32_FLOAT:
    case NYBLER_TEXTURE_FORMAT_DEPTH24_PLUS:
    case NYBLER_TEXTURE_FORMAT_DEPTH32_FLOAT: return 4;
    case NYBLER_TEXTURE_FORMAT_R16_FLOAT: return 2;
    case NYBLER_TEXTURE_FORMAT_RG32_FLOAT: return 8;
    case NYBLER_TEXTURE_FORMAT_RGBA32_FLOAT: return 16;
    default: return 0;
  }
}

NyblerResult nybler_device_create_buffer(
  NyblerDevice *device,
  const NyblerBufferDescriptor *descriptor,
  NyblerBuffer **out_buffer
) {
  NyblerBuffer *buffer;
  uint64_t allocation_size;

  if (!device || !descriptor || !out_buffer) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_buffer = NULL;
  if (descriptor->size == 0 || descriptor->usage == 0) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if ((descriptor->usage & NYBLER_BUFFER_USAGE_MAP_READ) &&
      (descriptor->usage & NYBLER_BUFFER_USAGE_MAP_WRITE)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  allocation_size = descriptor->size;
  if (allocation_size > (uint64_t)SIZE_MAX) return NYBLER_ERROR_OUT_OF_MEMORY;

  buffer = (NyblerBuffer *)calloc(1, sizeof(*buffer));
  if (!buffer) return NYBLER_ERROR_OUT_OF_MEMORY;
  buffer->data = (uint8_t *)calloc(1, (size_t)allocation_size);
  if (!buffer->data) {
    free(buffer);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  if (descriptor->initial_data) {
    memcpy(buffer->data, descriptor->initial_data, (size_t)descriptor->size);
  }
  if (descriptor->label) {
    buffer->label = nybler_internal_duplicate_string(descriptor->label);
    if (!buffer->label) {
      free(buffer->data);
      free(buffer);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }

  buffer->device = device;
  buffer->size = descriptor->size;
  buffer->usage = descriptor->usage;
  *out_buffer = buffer;
  return NYBLER_OK;
}

NyblerResult nybler_buffer_write(
  NyblerBuffer *buffer,
  uint64_t offset,
  const void *data,
  size_t size
) {
  if (!buffer || !data) return NYBLER_ERROR_INVALID_ARGUMENT;
  if (!nybler_buffer_fits(buffer, offset, (uint64_t)size)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (size) memcpy(buffer->data + offset, data, size);
  return NYBLER_OK;
}

NyblerResult nybler_buffer_read(
  const NyblerBuffer *buffer,
  uint64_t offset,
  void *data,
  size_t size
) {
  if (!buffer || !data) return NYBLER_ERROR_INVALID_ARGUMENT;
  if (!nybler_buffer_fits(buffer, offset, (uint64_t)size)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (size) memcpy(data, buffer->data + offset, size);
  return NYBLER_OK;
}

void nybler_buffer_release(NyblerBuffer *buffer) {
  if (!buffer) return;
  free(buffer->label);
  free(buffer->data);
  free(buffer);
}

NyblerResult nybler_device_create_texture(
  NyblerDevice *device,
  const NyblerTextureDescriptor *descriptor,
  NyblerTexture **out_texture
) {
  NyblerTexture *texture;
  NyblerResult result;

  if (!device || !descriptor || !out_texture) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_texture = NULL;
  if (descriptor->format == NYBLER_TEXTURE_FORMAT_UNDEFINED ||
      descriptor->size.width == 0 || descriptor->size.height == 0 ||
      descriptor->size.depth_or_array_layers == 0 ||
      descriptor->mip_level_count == 0 || descriptor->sample_count == 0 ||
      descriptor->usage == 0) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (descriptor->sample_count != 1 && descriptor->sample_count != 4) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  texture = (NyblerTexture *)calloc(1, sizeof(*texture));
  if (!texture) return NYBLER_ERROR_OUT_OF_MEMORY;
  texture->descriptor = *descriptor;
  if (descriptor->label) {
    texture->label = nybler_internal_duplicate_string(descriptor->label);
    if (!texture->label) {
      free(texture);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }
  texture->device = device;
  result = nybler_mesa_create_texture(
    (NyblerMesaContext *)device->backend_context, texture);
  if (result != NYBLER_OK) {
    free(texture->label);
    free(texture);
    return result;
  }
  *out_texture = texture;
  return NYBLER_OK;
}

void nybler_texture_release(NyblerTexture *texture) {
  if (!texture) return;
  nybler_mesa_destroy_texture(texture);
  free(texture->label);
  free(texture);
}

NyblerResult nybler_device_create_sampler(
  NyblerDevice *device,
  const NyblerSamplerDescriptor *descriptor,
  NyblerSampler **out_sampler
) {
  NyblerSampler *sampler;

  if (!device || !descriptor || !out_sampler) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_sampler = NULL;
  if (!nybler_in_range(descriptor->address_mode_u, NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       NYBLER_ADDRESS_MODE_MIRROR_REPEAT) ||
      !nybler_in_range(descriptor->address_mode_v, NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       NYBLER_ADDRESS_MODE_MIRROR_REPEAT) ||
      !nybler_in_range(descriptor->address_mode_w, NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                       NYBLER_ADDRESS_MODE_MIRROR_REPEAT) ||
      !nybler_in_range(descriptor->min_filter, NYBLER_FILTER_MODE_NEAREST,
                       NYBLER_FILTER_MODE_LINEAR) ||
      !nybler_in_range(descriptor->mag_filter, NYBLER_FILTER_MODE_NEAREST,
                       NYBLER_FILTER_MODE_LINEAR) ||
      !nybler_in_range(descriptor->mipmap_filter, NYBLER_FILTER_MODE_NEAREST,
                       NYBLER_FILTER_MODE_LINEAR) ||
      !nybler_in_range(descriptor->compare, NYBLER_COMPARE_FUNCTION_NONE,
                       NYBLER_COMPARE_FUNCTION_ALWAYS) ||
      descriptor->lod_min_clamp > descriptor->lod_max_clamp) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  sampler = (NyblerSampler *)calloc(1, sizeof(*sampler));
  if (!sampler) return NYBLER_ERROR_OUT_OF_MEMORY;
  sampler->descriptor = *descriptor;
  if (descriptor->label) {
    sampler->label = nybler_internal_duplicate_string(descriptor->label);
    if (!sampler->label) {
      free(sampler);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }
  sampler->device = device;
  *out_sampler = sampler;
  return NYBLER_OK;
}

void nybler_sampler_release(NyblerSampler *sampler) {
  if (!sampler) return;
  free(sampler->label);
  free(sampler);
}

NyblerResult nybler_device_create_bind_group_layout(
  NyblerDevice *device,
  const NyblerBindGroupLayoutDescriptor *descriptor,
  NyblerBindGroupLayout **out_layout
) {
  NyblerBindGroupLayout *layout;
  size_t i;
  size_t j;

  if (!device || !descriptor || !out_layout) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_layout = NULL;
  if (descriptor->entry_count == 0 || !descriptor->entries) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  for (i = 0; i < descriptor->entry_count; ++i) {
    const NyblerBindGroupLayoutEntry *entry = &descriptor->entries[i];
    if (entry->visibility == 0 ||
        !nybler_in_range(entry->binding_type, NYBLER_BINDING_TYPE_UNIFORM_BUFFER,
                         NYBLER_BINDING_TYPE_STORAGE_TEXTURE) ||
        (entry->has_dynamic_offset &&
         entry->binding_type != NYBLER_BINDING_TYPE_UNIFORM_BUFFER &&
         entry->binding_type != NYBLER_BINDING_TYPE_STORAGE_BUFFER &&
         entry->binding_type != NYBLER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER)) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
    for (j = 0; j < i; ++j) {
      if (descriptor->entries[j].binding == entry->binding) {
        return NYBLER_ERROR_INVALID_ARGUMENT;
      }
    }
  }

  layout = (NyblerBindGroupLayout *)calloc(1, sizeof(*layout));
  if (!layout) return NYBLER_ERROR_OUT_OF_MEMORY;
  layout->entries = (NyblerBindGroupLayoutEntry *)calloc(
    descriptor->entry_count, sizeof(*layout->entries));
  if (!layout->entries) {
    free(layout);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  memcpy(layout->entries, descriptor->entries,
         descriptor->entry_count * sizeof(*layout->entries));
  if (descriptor->label) {
    layout->label = nybler_internal_duplicate_string(descriptor->label);
    if (!layout->label) {
      free(layout->entries);
      free(layout);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }
  layout->device = device;
  layout->entry_count = descriptor->entry_count;
  *out_layout = layout;
  return NYBLER_OK;
}

void nybler_bind_group_layout_release(NyblerBindGroupLayout *layout) {
  if (!layout) return;
  free(layout->label);
  free(layout->entries);
  free(layout);
}

static const NyblerBindGroupLayoutEntry *nybler_layout_entry(
  const NyblerBindGroupLayout *layout,
  uint32_t binding
) {
  size_t i;

  if (!layout) return NULL;
  for (i = 0; i < layout->entry_count; ++i) {
    if (layout->entries[i].binding == binding) return &layout->entries[i];
  }
  return NULL;
}

NyblerResult nybler_device_create_bind_group(
  NyblerDevice *device,
  const NyblerBindGroupDescriptor *descriptor,
  NyblerBindGroup **out_group
) {
  NyblerBindGroup *group;
  size_t i;
  uint64_t size;

  if (!device || !descriptor || !out_group) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_group = NULL;
  if (!descriptor->layout || !descriptor->entries ||
      descriptor->entry_count != descriptor->layout->entry_count) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  for (i = 0; i < descriptor->entry_count; ++i) {
    const NyblerBindGroupEntry *entry = &descriptor->entries[i];
    const NyblerBindGroupLayoutEntry *layout_entry =
      nybler_layout_entry(descriptor->layout, entry->binding);
    int is_buffer = layout_entry &&
      layout_entry->binding_type >= NYBLER_BINDING_TYPE_UNIFORM_BUFFER &&
      layout_entry->binding_type <= NYBLER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER;
    int is_sampler = layout_entry &&
      (layout_entry->binding_type == NYBLER_BINDING_TYPE_SAMPLER ||
       layout_entry->binding_type == NYBLER_BINDING_TYPE_COMPARISON_SAMPLER);
    int is_texture = layout_entry &&
      (layout_entry->binding_type == NYBLER_BINDING_TYPE_TEXTURE ||
       layout_entry->binding_type == NYBLER_BINDING_TYPE_STORAGE_TEXTURE);

    if (!layout_entry) return NYBLER_ERROR_INVALID_ARGUMENT;
    if (is_buffer && (!entry->buffer ||
        !nybler_buffer_fits(entry->buffer, entry->offset, entry->size))) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
    if (is_sampler && !entry->sampler) return NYBLER_ERROR_INVALID_ARGUMENT;
    if (is_texture && !entry->texture) return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  group = (NyblerBindGroup *)calloc(1, sizeof(*group));
  if (!group) return NYBLER_ERROR_OUT_OF_MEMORY;
  group->entries = (NyblerBindGroupEntryStorage *)calloc(
    descriptor->entry_count, sizeof(*group->entries));
  if (!group->entries) {
    free(group);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  for (i = 0; i < descriptor->entry_count; ++i) {
    const NyblerBindGroupEntry *entry = &descriptor->entries[i];
    const NyblerBindGroupLayoutEntry *layout_entry =
      nybler_layout_entry(descriptor->layout, entry->binding);
    size = entry->size;
    if (entry->buffer && size == 0) size = entry->buffer->size - entry->offset;
    group->entries[i].binding = entry->binding;
    group->entries[i].buffer = entry->buffer;
    group->entries[i].offset = entry->offset;
    group->entries[i].size = size;
    group->entries[i].texture = entry->texture;
    group->entries[i].sampler = entry->sampler;
    group->entries[i].binding_type = layout_entry->binding_type;
  }
  if (descriptor->label) {
    group->label = nybler_internal_duplicate_string(descriptor->label);
    if (!group->label) {
      free(group->entries);
      free(group);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }

  group->device = device;
  group->layout = descriptor->layout;
  group->entry_count = descriptor->entry_count;
  *out_group = group;
  return NYBLER_OK;
}

void nybler_bind_group_release(NyblerBindGroup *group) {
  if (!group) return;
  free(group->label);
  free(group->entries);
  free(group);
}

/* Generate the GLSL ES 300 programs that the Mesa backend compiles. The
 * binding map comes from this pipeline's layout, so the generated code is a
 * translation of these exact modules and entry points, not a fixed program. */
static NyblerResult nybler_pipeline_translate_glsl(NyblerRenderPipeline *pipeline) {
  NyblerGlslBinding *bindings = NULL;
  size_t binding_count = 0;
  uint8_t *code = NULL;
  size_t code_size = 0;
  char *error = NULL;
  NyblerResult result;
  size_t i;

  if (!pipeline->vertex_module->source || !pipeline->fragment_module->source) {
    return NYBLER_ERROR_UNSUPPORTED;
  }
  if (pipeline->layout) {
    binding_count = pipeline->layout->entry_count;
    if (binding_count) {
      bindings = (NyblerGlslBinding *)calloc(
        binding_count, sizeof(*bindings));
      if (!bindings) return NYBLER_ERROR_OUT_OF_MEMORY;
      for (i = 0; i < binding_count; ++i) {
        bindings[i].group = 0;
        bindings[i].binding = pipeline->layout->entries[i].binding;
        bindings[i].glsl_binding = pipeline->layout->entries[i].binding;
        bindings[i].type = pipeline->layout->entries[i].binding_type;
      }
    }
  }

  result = nybler_tint_translate_glsl(
    pipeline->vertex_module->source,
    pipeline->vertex_module->source_size,
    pipeline->vertex_entry_point,
    NYBLER_GLSL_STAGE_VERTEX,
    bindings,
    binding_count,
    &code,
    &code_size,
    &error
  );
  nybler_string_release(error);
  error = NULL;
  if (result != NYBLER_OK) goto fail;
  pipeline->vertex_glsl = (char *)code;
  pipeline->vertex_glsl_size = code_size;
  code = NULL;

  result = nybler_tint_translate_glsl(
    pipeline->fragment_module->source,
    pipeline->fragment_module->source_size,
    pipeline->fragment_entry_point,
    NYBLER_GLSL_STAGE_FRAGMENT,
    bindings,
    binding_count,
    &code,
    &code_size,
    &error
  );
  nybler_string_release(error);
  if (result != NYBLER_OK) goto fail;
  pipeline->fragment_glsl = (char *)code;
  pipeline->fragment_glsl_size = code_size;
  code = NULL;

  free(bindings);
  return NYBLER_OK;

fail:
  nybler_shader_code_release(code);
  free(bindings);
  return result;
}

NyblerResult nybler_device_create_render_pipeline(
  NyblerDevice *device,
  const NyblerRenderPipelineDescriptor *descriptor,
  NyblerRenderPipeline **out_pipeline
) {
  NyblerRenderPipeline *pipeline;
  NyblerResult result;
  size_t i;

  if (!device || !descriptor || !out_pipeline) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_pipeline = NULL;
  if (!descriptor->vertex_module || !descriptor->fragment_module ||
      !descriptor->vertex_entry_point || !descriptor->fragment_entry_point ||
      !descriptor->targets || descriptor->target_count == 0 ||
      !nybler_in_range(descriptor->topology,
                       NYBLER_PRIMITIVE_TOPOLOGY_POINT_LIST,
                       NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (descriptor->vertex_module->format != NYBLER_SHADER_FORMAT_SPIRV ||
      descriptor->fragment_module->format != NYBLER_SHADER_FORMAT_SPIRV) {
    return NYBLER_ERROR_UNSUPPORTED;
  }
  for (i = 0; i < descriptor->target_count; ++i) {
    if (descriptor->targets[i].format == NYBLER_TEXTURE_FORMAT_UNDEFINED) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
  }
  if (descriptor->vertex_attribute_count) {
    if (!descriptor->vertex_attributes || descriptor->vertex_stride == 0) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0; i < descriptor->vertex_attribute_count; ++i) {
      const NyblerVertexAttribute *attribute = &descriptor->vertex_attributes[i];
      uint32_t size = nybler_vertex_format_size(attribute->format);
      if (!size || attribute->offset > descriptor->vertex_stride ||
          size > descriptor->vertex_stride - attribute->offset) {
        return NYBLER_ERROR_INVALID_ARGUMENT;
      }
    }
  }

  pipeline = (NyblerRenderPipeline *)calloc(1, sizeof(*pipeline));
  if (!pipeline) return NYBLER_ERROR_OUT_OF_MEMORY;
  pipeline->targets = (NyblerColorTargetState *)calloc(
    descriptor->target_count, sizeof(*pipeline->targets));
  if (!pipeline->targets) {
    free(pipeline);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  memcpy(pipeline->targets, descriptor->targets,
         descriptor->target_count * sizeof(*pipeline->targets));
  if (descriptor->vertex_attribute_count) {
    pipeline->vertex_attributes = (NyblerVertexAttribute *)calloc(
      descriptor->vertex_attribute_count,
      sizeof(*pipeline->vertex_attributes));
    if (!pipeline->vertex_attributes) {
      nybler_render_pipeline_release(pipeline);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
    memcpy(
      pipeline->vertex_attributes,
      descriptor->vertex_attributes,
      descriptor->vertex_attribute_count * sizeof(*pipeline->vertex_attributes));
    pipeline->vertex_attribute_count = descriptor->vertex_attribute_count;
    pipeline->vertex_stride = descriptor->vertex_stride;
  }

  pipeline->vertex_entry_point =
    nybler_internal_duplicate_string(descriptor->vertex_entry_point);
  pipeline->fragment_entry_point =
    nybler_internal_duplicate_string(descriptor->fragment_entry_point);
  if (descriptor->label) {
    pipeline->label = nybler_internal_duplicate_string(descriptor->label);
  }
  if (!pipeline->vertex_entry_point || !pipeline->fragment_entry_point ||
      (descriptor->label && !pipeline->label)) {
    nybler_render_pipeline_release(pipeline);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  pipeline->device = device;
  pipeline->vertex_module = descriptor->vertex_module;
  pipeline->fragment_module = descriptor->fragment_module;
  pipeline->target_count = descriptor->target_count;
  pipeline->topology = descriptor->topology;
  pipeline->layout = descriptor->layout;
  if (device->backend == NYBLER_BACKEND_MESA_GL && device->backend_context) {
    result = nybler_pipeline_translate_glsl(pipeline);
    if (result != NYBLER_OK) {
      nybler_render_pipeline_release(pipeline);
      return result;
    }
  }
  result = nybler_mesa_create_pipeline(
    (NyblerMesaContext *)device->backend_context, pipeline);
  if (result != NYBLER_OK) {
    nybler_render_pipeline_release(pipeline);
    return result;
  }
  *out_pipeline = pipeline;
  return NYBLER_OK;
}

void nybler_render_pipeline_release(NyblerRenderPipeline *pipeline) {
  if (!pipeline) return;
  nybler_mesa_destroy_pipeline(pipeline);
  free(pipeline->vertex_glsl);
  free(pipeline->fragment_glsl);
  free(pipeline->label);
  free(pipeline->vertex_entry_point);
  free(pipeline->fragment_entry_point);
  free(pipeline->targets);
  free(pipeline->vertex_attributes);
  free(pipeline);
}

static NyblerResult nybler_encoder_reserve(NyblerCommandEncoder *encoder) {
  NyblerCommand *grown;
  size_t capacity;

  if (encoder->command_count < encoder->command_capacity) return NYBLER_OK;
  capacity = encoder->command_capacity ? encoder->command_capacity * 2 : 8;
  if (capacity < encoder->command_capacity ||
      capacity > SIZE_MAX / sizeof(*grown)) {
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  grown = (NyblerCommand *)realloc(
    encoder->commands, capacity * sizeof(*grown));
  if (!grown) return NYBLER_ERROR_OUT_OF_MEMORY;
  encoder->commands = grown;
  encoder->command_capacity = capacity;
  return NYBLER_OK;
}

static NyblerResult nybler_encoder_append(
  NyblerCommandEncoder *encoder,
  const NyblerCommand *command
) {
  NyblerResult result = nybler_encoder_reserve(encoder);

  if (result != NYBLER_OK) return result;
  encoder->commands[encoder->command_count++] = *command;
  return NYBLER_OK;
}

static void nybler_free_render_pass_command(NyblerCommand *command) {
  free(command->data.render_pass.attachments);
  free(command->data.render_pass.label);
  memset(command, 0, sizeof(*command));
}

NyblerResult nybler_device_create_command_encoder(
  NyblerDevice *device,
  NyblerCommandEncoder **out_encoder
) {
  NyblerCommandEncoder *encoder;

  if (!device || !out_encoder) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_encoder = NULL;
  encoder = (NyblerCommandEncoder *)calloc(1, sizeof(*encoder));
  if (!encoder) return NYBLER_ERROR_OUT_OF_MEMORY;
  encoder->device = device;
  *out_encoder = encoder;
  return NYBLER_OK;
}

NyblerResult nybler_command_encoder_begin_render_pass(
  NyblerCommandEncoder *encoder,
  const NyblerRenderPassDescriptor *descriptor,
  NyblerRenderPassEncoder **out_pass
) {
  NyblerCommand command;
  NyblerRenderPassEncoder *pass;
  size_t i;

  if (!encoder || !descriptor || !out_pass) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_pass = NULL;
  if (encoder->active_pass && !encoder->active_pass->ended) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (descriptor->color_attachment_count == 0 || !descriptor->color_attachments) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  for (i = 0; i < descriptor->color_attachment_count; ++i) {
    const NyblerRenderPassColorAttachment *attachment =
      &descriptor->color_attachments[i];
    if (!attachment->texture ||
        !(attachment->texture->descriptor.usage &
          NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT) ||
        !nybler_in_range(attachment->load_op, NYBLER_LOAD_OP_LOAD,
                         NYBLER_LOAD_OP_DONT_CARE) ||
        !nybler_in_range(attachment->store_op, NYBLER_STORE_OP_STORE,
                         NYBLER_STORE_OP_DISCARD)) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
  }

  memset(&command, 0, sizeof(command));
  command.kind = NYBLER_COMMAND_BEGIN_RENDER_PASS;
  command.data.render_pass.attachments =
    (NyblerRenderPassColorAttachment *)calloc(
      descriptor->color_attachment_count,
      sizeof(*command.data.render_pass.attachments));
  if (!command.data.render_pass.attachments) return NYBLER_ERROR_OUT_OF_MEMORY;
  memcpy(command.data.render_pass.attachments, descriptor->color_attachments,
         descriptor->color_attachment_count *
         sizeof(*command.data.render_pass.attachments));
  if (descriptor->label) {
    command.data.render_pass.label =
      nybler_internal_duplicate_string(descriptor->label);
    if (!command.data.render_pass.label) {
      free(command.data.render_pass.attachments);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }
  command.data.render_pass.attachment_count = descriptor->color_attachment_count;

  if (nybler_encoder_append(encoder, &command) != NYBLER_OK) {
    nybler_free_render_pass_command(&command);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }

  free(encoder->active_pass);
  encoder->active_pass = (NyblerRenderPassEncoder *)calloc(
    1, sizeof(*encoder->active_pass));
  if (!encoder->active_pass) {
    encoder->command_count--;
    nybler_free_render_pass_command(&command);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  encoder->active_pass->encoder = encoder;
  encoder->active_pass->ended = 0;
  *out_pass = encoder->active_pass;
  return NYBLER_OK;
}

static NyblerResult nybler_append_pass_command(
  NyblerRenderPassEncoder *pass,
  NyblerCommandKind kind,
  const NyblerCommand *command
) {
  NyblerCommandEncoder *encoder;
  NyblerResult result;

  if (!pass || pass->ended || !pass->encoder) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  encoder = pass->encoder;
  result = nybler_encoder_reserve(encoder);
  if (result != NYBLER_OK) return result;
  encoder->commands[encoder->command_count] = *command;
  encoder->commands[encoder->command_count].kind = kind;
  encoder->command_count++;
  return NYBLER_OK;
}

void nybler_render_pass_set_pipeline(
  NyblerRenderPassEncoder *pass,
  NyblerRenderPipeline *pipeline
) {
  NyblerCommand command;

  if (!pipeline) return;
  memset(&command, 0, sizeof(command));
  command.data.set_pipeline.pipeline = pipeline;
  (void)nybler_append_pass_command(
    pass, NYBLER_COMMAND_SET_PIPELINE, &command);
}

void nybler_render_pass_set_bind_group(
  NyblerRenderPassEncoder *pass,
  uint32_t index,
  NyblerBindGroup *group
) {
  NyblerCommand command;

  if (index >= 32 || !group) return;
  memset(&command, 0, sizeof(command));
  command.data.set_bind_group.index = index;
  command.data.set_bind_group.group = group;
  (void)nybler_append_pass_command(
    pass, NYBLER_COMMAND_SET_BIND_GROUP, &command);
}

void nybler_render_pass_set_vertex_buffer(
  NyblerRenderPassEncoder *pass,
  uint32_t slot,
  NyblerBuffer *buffer,
  uint64_t offset
) {
  NyblerCommand command;

  if (slot >= 8 || !buffer || !(buffer->usage & NYBLER_BUFFER_USAGE_VERTEX) ||
      !nybler_buffer_fits(buffer, offset, 0)) {
    return;
  }
  memset(&command, 0, sizeof(command));
  command.data.set_vertex_buffer.slot = slot;
  command.data.set_vertex_buffer.buffer = buffer;
  command.data.set_vertex_buffer.offset = offset;
  (void)nybler_append_pass_command(
    pass, NYBLER_COMMAND_SET_VERTEX_BUFFER, &command);
}

void nybler_render_pass_draw(
  NyblerRenderPassEncoder *pass,
  uint32_t vertex_count,
  uint32_t instance_count,
  uint32_t first_vertex,
  uint32_t first_instance
) {
  NyblerCommand command;

  if (vertex_count == 0 || instance_count == 0) return;
  memset(&command, 0, sizeof(command));
  command.data.draw.vertex_count = vertex_count;
  command.data.draw.instance_count = instance_count;
  command.data.draw.first_vertex = first_vertex;
  command.data.draw.first_instance = first_instance;
  (void)nybler_append_pass_command(pass, NYBLER_COMMAND_DRAW, &command);
}

NyblerResult nybler_command_encoder_copy_texture_to_buffer(
  NyblerCommandEncoder *encoder,
  const NyblerImageCopyTexture *source,
  const NyblerImageCopyBuffer *destination,
  const NyblerExtent3D *copy_size
) {
  NyblerCommand command;
  uint32_t bytes_per_pixel;
  uint64_t row_size;
  uint64_t required;
  const NyblerTextureDescriptor *descriptor;

  if (!encoder || !source || !destination || !copy_size) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (encoder->active_pass && !encoder->active_pass->ended) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  descriptor = source->texture ? &source->texture->descriptor : NULL;
  if (!descriptor ||
      !(descriptor->usage & NYBLER_TEXTURE_USAGE_COPY_SRC) ||
      !destination->buffer ||
      !(destination->buffer->usage & NYBLER_BUFFER_USAGE_COPY_DST)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (source->mip_level >= descriptor->mip_level_count) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  bytes_per_pixel = nybler_texture_bytes_per_pixel(descriptor->format);
  if (!bytes_per_pixel || copy_size->width == 0 || copy_size->height == 0 ||
      copy_size->depth_or_array_layers == 0 ||
      (uint64_t)source->origin.x + copy_size->width > descriptor->size.width ||
      (uint64_t)source->origin.y + copy_size->height > descriptor->size.height ||
      (uint64_t)source->origin.z + copy_size->depth_or_array_layers >
        descriptor->size.depth_or_array_layers) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  row_size = (uint64_t)copy_size->width * bytes_per_pixel;
  if (destination->bytes_per_row < row_size ||
      (destination->rows_per_image &&
       destination->rows_per_image < copy_size->height)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  required = ((uint64_t)copy_size->height - 1) * destination->bytes_per_row +
    row_size;
  if (!nybler_buffer_fits(destination->buffer, destination->offset, required)) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  memset(&command, 0, sizeof(command));
  command.kind = NYBLER_COMMAND_COPY_TEXTURE_TO_BUFFER;
  command.data.copy_texture_to_buffer.source = *source;
  command.data.copy_texture_to_buffer.destination = *destination;
  command.data.copy_texture_to_buffer.size = *copy_size;
  return nybler_encoder_append(encoder, &command);
}

void nybler_render_pass_end(NyblerRenderPassEncoder *pass) {
  NyblerCommand command;

  if (!pass || pass->ended || !pass->encoder) return;
  memset(&command, 0, sizeof(command));
  if (nybler_append_pass_command(
        pass, NYBLER_COMMAND_END_RENDER_PASS, &command) != NYBLER_OK) {
    return;
  }
  pass->ended = 1;
}

NyblerResult nybler_command_encoder_finish(
  NyblerCommandEncoder *encoder,
  NyblerCommandBuffer **out_commands
) {
  NyblerCommandBuffer *commands;

  if (!encoder || !out_commands) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_commands = NULL;
  if (encoder->active_pass && !encoder->active_pass->ended) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  if (encoder->command_count == 0) return NYBLER_ERROR_INVALID_ARGUMENT;

  commands = (NyblerCommandBuffer *)calloc(1, sizeof(*commands));
  if (!commands) return NYBLER_ERROR_OUT_OF_MEMORY;
  commands->commands = encoder->commands;
  commands->command_count = encoder->command_count;
  commands->device = encoder->device;

  encoder->commands = NULL;
  encoder->command_count = 0;
  encoder->command_capacity = 0;
  free(encoder->active_pass);
  encoder->active_pass = NULL;

  *out_commands = commands;
  return NYBLER_OK;
}

void nybler_command_encoder_release(NyblerCommandEncoder *encoder) {
  if (!encoder) return;
  free(encoder->active_pass);
  nybler_internal_free_commands(encoder->commands, encoder->command_count);
  free(encoder);
}

void nybler_command_buffer_release(NyblerCommandBuffer *commands) {
  if (!commands) return;
  nybler_internal_free_commands(commands->commands, commands->command_count);
  free(commands);
}

NyblerQueue *nybler_device_get_queue(NyblerDevice *device) {
  return device ? device->queue : NULL;
}

NyblerResult nybler_queue_submit(
  NyblerQueue *queue,
  size_t command_count,
  NyblerCommandBuffer *const *commands
) {
  NyblerResult result;
  size_t i;

  if (!queue || !queue->device || !commands) return NYBLER_ERROR_INVALID_ARGUMENT;
  for (i = 0; i < command_count; ++i) {
    if (!commands[i] || commands[i]->device != queue->device) {
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
  }

  result = NYBLER_OK;
  if (queue->device->backend_context) {
    result = nybler_mesa_execute(
      (NyblerMesaContext *)queue->device->backend_context,
      command_count,
      commands
    );
  }

  /* Commands are validated and consumed here. */
  for (i = 0; i < command_count; ++i) {
    nybler_command_buffer_release(commands[i]);
  }
  return result;
}
