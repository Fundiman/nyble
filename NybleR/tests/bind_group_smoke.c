#include "nybler/nybler.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "bind group smoke failed: %s\n", message);
  return 0;
}

int main(void) {
  static const char shader_source[] =
    "struct Params {\n"
    "  color : vec4<f32>,\n"
    "  offset : vec2<f32>,\n"
    "  pad0 : u32,\n"
    "  pad1 : u32,\n"
    "};\n"
    "@group(0) @binding(0) var<uniform> params : Params;\n"
    "@group(0) @binding(1) var samp : sampler;\n"
    "@group(0) @binding(2) var tex : texture_2d<f32>;\n"
    "struct VertexOut {\n"
    "  @builtin(position) position : vec4<f32>,\n"
    "  @location(0) uv : vec2<f32>,\n"
    "};\n"
    "@vertex\n"
    "fn vs_main(\n"
    "  @location(0) position : vec2<f32>,\n"
    "  @location(1) uv : vec2<f32>\n"
    ") -> VertexOut {\n"
    "  var out : VertexOut;\n"
    "  out.position = vec4<f32>(position + params.offset, 0.0, 1.0);\n"
    "  out.uv = uv;\n"
    "  return out;\n"
    "}\n"
    "@fragment\n"
    "fn fs_main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {\n"
    "  return textureSample(tex, samp, uv) * params.color;\n"
    "}\n";
  static const float vertices[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f
  };
  static const float uniform_data[8] = {
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 0.0f, 0.0f
  };
  static const uint8_t white_pixels[4 * 4 * 4] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
  };
  static const size_t width = 4;
  static const size_t height = 4;
  static const size_t readback_size = width * height * 4;

  NyblerInitOptions options = {1, NYBLER_BACKEND_MESA_GL, 1, NULL};
  NyblerBindGroupLayoutEntry layout_entries[3];
  NyblerBindGroupLayoutDescriptor layout_descriptor;
  NyblerBindGroupEntry bind_entries[3];
  NyblerBindGroupDescriptor bind_descriptor;
  NyblerBufferDescriptor buffer_descriptor;
  NyblerTextureDescriptor texture_descriptor;
  NyblerSamplerDescriptor sampler_descriptor;
  NyblerColorTargetState target = {NYBLER_TEXTURE_FORMAT_RGBA8_UNORM, 0};
  NyblerVertexAttribute attributes[2];
  NyblerRenderPipelineDescriptor pipeline_descriptor;
  NyblerRenderPassColorAttachment attachment;
  NyblerRenderPassDescriptor pass_descriptor;
  NyblerImageCopyTexture copy_source;
  NyblerImageCopyBuffer copy_destination;
  NyblerExtent3D copy_size = {width, height, 1};
  uint8_t pixels[readback_size];

  NyblerInstance *instance = NULL;
  NyblerAdapter *adapter = NULL;
  NyblerDevice *device = NULL;
  NyblerQueue *queue = NULL;
  NyblerBuffer *vertices_buffer = NULL;
  NyblerBuffer *uniform_buffer = NULL;
  NyblerBuffer *readback = NULL;
  NyblerTexture *source_texture = NULL;
  NyblerTexture *target_texture = NULL;
  NyblerSampler *sampler = NULL;
  NyblerBindGroupLayout *layout = NULL;
  NyblerBindGroup *group = NULL;
  NyblerShaderModule *vertex_module = NULL;
  NyblerShaderModule *fragment_module = NULL;
  NyblerRenderPipeline *pipeline = NULL;
  NyblerCommandEncoder *encoder = NULL;
  NyblerRenderPassEncoder *pass = NULL;
  NyblerCommandBuffer *commands = NULL;
  int ok = 1;
  size_t i;

  memset(pixels, 0, sizeof(pixels));
  ok = check(nybler_init(&options, &instance) == NYBLER_OK, "init") && ok;
  ok = check(
    nybler_instance_request_adapter(instance, &adapter) == NYBLER_OK,
    "request adapter"
  ) && ok;
  ok = check(
    nybler_adapter_request_device(adapter, &device) == NYBLER_OK,
    "request device"
  ) && ok;
  queue = nybler_device_get_queue(device);
  ok = check(queue != NULL, "get queue") && ok;

  memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));
  buffer_descriptor.size = sizeof(vertices);
  buffer_descriptor.usage = NYBLER_BUFFER_USAGE_VERTEX;
  buffer_descriptor.initial_data = vertices;
  ok = check(
    nybler_device_create_buffer(device, &buffer_descriptor, &vertices_buffer) == NYBLER_OK,
    "create vertex buffer"
  ) && ok;

  memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));
  buffer_descriptor.size = sizeof(uniform_data);
  buffer_descriptor.usage = NYBLER_BUFFER_USAGE_UNIFORM;
  buffer_descriptor.initial_data = uniform_data;
  ok = check(
    nybler_device_create_buffer(device, &buffer_descriptor, &uniform_buffer) == NYBLER_OK,
    "create uniform buffer"
  ) && ok;

  memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));
  buffer_descriptor.size = readback_size;
  buffer_descriptor.usage = NYBLER_BUFFER_USAGE_COPY_DST;
  ok = check(
    nybler_device_create_buffer(device, &buffer_descriptor, &readback) == NYBLER_OK,
    "create readback buffer"
  ) && ok;

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size.width = width;
  texture_descriptor.size.height = height;
  texture_descriptor.size.depth_or_array_layers = 1;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage = NYBLER_TEXTURE_USAGE_TEXTURE_BINDING;
  texture_descriptor.initial_data = white_pixels;
  ok = check(
    nybler_device_create_texture(device, &texture_descriptor, &source_texture) == NYBLER_OK,
    "create source texture"
  ) && ok;

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size.width = width;
  texture_descriptor.size.height = height;
  texture_descriptor.size.depth_or_array_layers = 1;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage =
    NYBLER_TEXTURE_USAGE_COPY_SRC | NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT;
  ok = check(
    nybler_device_create_texture(device, &texture_descriptor, &target_texture) == NYBLER_OK,
    "create target texture"
  ) && ok;

  memset(&sampler_descriptor, 0, sizeof(sampler_descriptor));
  sampler_descriptor.address_mode_u = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.address_mode_v = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.address_mode_w = NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_descriptor.min_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.mag_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.mipmap_filter = NYBLER_FILTER_MODE_NEAREST;
  sampler_descriptor.lod_max_clamp = 1.0f;
  ok = check(
    nybler_device_create_sampler(device, &sampler_descriptor, &sampler) == NYBLER_OK,
    "create sampler"
  ) && ok;

  memset(layout_entries, 0, sizeof(layout_entries));
  layout_entries[0].binding = 0;
  layout_entries[0].visibility = NYBLER_SHADER_STAGE_VERTEX | NYBLER_SHADER_STAGE_FRAGMENT;
  layout_entries[0].binding_type = NYBLER_BINDING_TYPE_UNIFORM_BUFFER;
  layout_entries[1].binding = 1;
  layout_entries[1].visibility = NYBLER_SHADER_STAGE_FRAGMENT;
  layout_entries[1].binding_type = NYBLER_BINDING_TYPE_SAMPLER;
  layout_entries[2].binding = 2;
  layout_entries[2].visibility = NYBLER_SHADER_STAGE_FRAGMENT;
  layout_entries[2].binding_type = NYBLER_BINDING_TYPE_TEXTURE;
  memset(&layout_descriptor, 0, sizeof(layout_descriptor));
  layout_descriptor.entries = layout_entries;
  layout_descriptor.entry_count = 3;
  ok = check(
    nybler_device_create_bind_group_layout(device, &layout_descriptor, &layout) == NYBLER_OK,
    "create bind group layout"
  ) && ok;

  memset(bind_entries, 0, sizeof(bind_entries));
  bind_entries[0].binding = 0;
  bind_entries[0].buffer = uniform_buffer;
  bind_entries[1].binding = 1;
  bind_entries[1].sampler = sampler;
  bind_entries[2].binding = 2;
  bind_entries[2].texture = source_texture;
  memset(&bind_descriptor, 0, sizeof(bind_descriptor));
  bind_descriptor.layout = layout;
  bind_descriptor.entries = bind_entries;
  bind_descriptor.entry_count = 3;
  ok = check(
    nybler_device_create_bind_group(device, &bind_descriptor, &group) == NYBLER_OK,
    "create bind group"
  ) && ok;

  ok = check(
    nybler_shader_module_create(
      device, NYBLER_SHADER_FORMAT_WGSL, shader_source,
      sizeof(shader_source) - 1, "vs", &vertex_module) == NYBLER_OK,
    "create vertex module"
  ) && ok;
  ok = check(
    nybler_shader_module_create(
      device, NYBLER_SHADER_FORMAT_WGSL, shader_source,
      sizeof(shader_source) - 1, "fs", &fragment_module) == NYBLER_OK,
    "create fragment module"
  ) && ok;

  memset(attributes, 0, sizeof(attributes));
  attributes[0].format = NYBLER_VERTEX_FORMAT_FLOAT32X2;
  attributes[0].offset = 0;
  attributes[0].shader_location = 0;
  attributes[1].format = NYBLER_VERTEX_FORMAT_FLOAT32X2;
  attributes[1].offset = 8;
  attributes[1].shader_location = 1;
  memset(&pipeline_descriptor, 0, sizeof(pipeline_descriptor));
  pipeline_descriptor.vertex_module = vertex_module;
  pipeline_descriptor.fragment_module = fragment_module;
  pipeline_descriptor.vertex_entry_point = "vs_main";
  pipeline_descriptor.fragment_entry_point = "fs_main";
  pipeline_descriptor.targets = &target;
  pipeline_descriptor.target_count = 1;
  pipeline_descriptor.topology = NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_descriptor.layout = layout;
  pipeline_descriptor.vertex_attributes = attributes;
  pipeline_descriptor.vertex_attribute_count = 2;
  pipeline_descriptor.vertex_stride = 16;
  ok = check(
    nybler_device_create_render_pipeline(device, &pipeline_descriptor, &pipeline) == NYBLER_OK,
    "create render pipeline"
  ) && ok;

  ok = check(
    nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
    "create command encoder"
  ) && ok;
  memset(&attachment, 0, sizeof(attachment));
  attachment.texture = target_texture;
  attachment.clear_value = (NyblerColor){0.0f, 0.0f, 0.0f, 1.0f};
  attachment.load_op = NYBLER_LOAD_OP_CLEAR;
  attachment.store_op = NYBLER_STORE_OP_STORE;
  memset(&pass_descriptor, 0, sizeof(pass_descriptor));
  pass_descriptor.color_attachments = &attachment;
  pass_descriptor.color_attachment_count = 1;
  ok = check(
    nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &pass) == NYBLER_OK,
    "begin render pass"
  ) && ok;
  nybler_render_pass_set_pipeline(pass, pipeline);
  nybler_render_pass_set_bind_group(pass, 0, group);
  nybler_render_pass_set_vertex_buffer(pass, 0, vertices_buffer, 0);
  nybler_render_pass_draw(pass, 6, 1, 0, 0);
  nybler_render_pass_end(pass);

  memset(&copy_source, 0, sizeof(copy_source));
  copy_source.texture = target_texture;
  memset(&copy_destination, 0, sizeof(copy_destination));
  copy_destination.buffer = readback;
  copy_destination.bytes_per_row = width * 4;
  ok = check(
    nybler_command_encoder_copy_texture_to_buffer(
      encoder, &copy_source, &copy_destination, &copy_size) == NYBLER_OK,
    "copy texture to buffer"
  ) && ok;
  ok = check(
    nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
    "finish command buffer"
  ) && ok;
  ok = check(
    nybler_queue_submit(queue, 1, &commands) == NYBLER_OK,
    "submit command buffer"
  ) && ok;
  commands = NULL;
  ok = check(
    nybler_buffer_read(readback, 0, pixels, readback_size) == NYBLER_OK,
    "read pixels"
  ) && ok;

  for (i = 0; i < width * height; ++i) {
    const uint8_t *pixel = &pixels[i * 4];
    ok = check(
      pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255,
      "uniform color applied"
    ) && ok;
  }

  /* A second value proves the draw reads the buffer, not a baked constant. */
  ok = check(
    nybler_buffer_write(uniform_buffer, 0, (const float[4]){1.0f, 0.0f, 0.0f, 1.0f}, 16) == NYBLER_OK,
    "rewrite uniform buffer"
  ) && ok;
  ok = check(
    nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
    "create second command encoder"
  ) && ok;
  memset(&pass_descriptor, 0, sizeof(pass_descriptor));
  pass_descriptor.color_attachments = &attachment;
  pass_descriptor.color_attachment_count = 1;
  ok = check(
    nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &pass) == NYBLER_OK,
    "begin second render pass"
  ) && ok;
  nybler_render_pass_set_pipeline(pass, pipeline);
  nybler_render_pass_set_bind_group(pass, 0, group);
  nybler_render_pass_set_vertex_buffer(pass, 0, vertices_buffer, 0);
  nybler_render_pass_draw(pass, 6, 1, 0, 0);
  nybler_render_pass_end(pass);
  ok = check(
    nybler_command_encoder_copy_texture_to_buffer(
      encoder, &copy_source, &copy_destination, &copy_size) == NYBLER_OK,
    "second copy texture to buffer"
  ) && ok;
  ok = check(
    nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
    "finish second command buffer"
  ) && ok;
  ok = check(
    nybler_queue_submit(queue, 1, &commands) == NYBLER_OK,
    "submit second command buffer"
  ) && ok;
  commands = NULL;
  memset(pixels, 0, sizeof(pixels));
  ok = check(
    nybler_buffer_read(readback, 0, pixels, readback_size) == NYBLER_OK,
    "read second pixels"
  ) && ok;
  for (i = 0; i < width * height; ++i) {
    const uint8_t *pixel = &pixels[i * 4];
    ok = check(
      pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] == 255,
      "rewritten uniform color applied"
    ) && ok;
  }

  if (ok) {
    puts("nybler bind group smoke: uniform buffer/sampler/texture ok");
  }

  nybler_command_encoder_release(encoder);
  nybler_render_pipeline_release(pipeline);
  nybler_shader_module_release(fragment_module);
  nybler_shader_module_release(vertex_module);
  nybler_bind_group_release(group);
  nybler_bind_group_layout_release(layout);
  nybler_sampler_release(sampler);
  nybler_texture_release(source_texture);
  nybler_texture_release(target_texture);
  nybler_buffer_release(readback);
  nybler_buffer_release(uniform_buffer);
  nybler_buffer_release(vertices_buffer);
  nybler_device_release(device);
  nybler_adapter_release(adapter);
  nybler_shutdown(instance);
  return ok ? 0 : 1;
}
