#include "nybler/nybler.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "resources smoke failed: %s\n", message);
  return 0;
}

int main(void) {
  static const char shader_source[] =
    "@vertex fn vs_main() -> @builtin(position) vec4<f32> {\n"
    "  return vec4<f32>(0.0, 0.0, 0.0, 1.0);\n"
    "}\n"
    "@fragment fn fs_main() -> @location(0) vec4<f32> {\n"
    "  return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
    "}\n";
  static const float initial_vertices[] = {0.0f, 0.0f, 0.0f, 1.0f};
  static const float uniform_value = 1.0f;

  NyblerInitOptions options = {1, NYBLER_BACKEND_AUTO, 1, NULL};
  NyblerBindGroupLayoutEntry layout_entry = {0, NYBLER_SHADER_STAGE_VERTEX,
                                              NYBLER_BINDING_TYPE_UNIFORM_BUFFER, 0};
  NyblerBindGroupLayoutDescriptor layout_descriptor = {&layout_entry, 1, "layout"};
  NyblerBindGroupEntry bind_entry;
  NyblerBindGroupDescriptor bind_descriptor;
  NyblerColorTargetState target = {NYBLER_TEXTURE_FORMAT_RGBA8_UNORM, 0};
  NyblerRenderPipelineDescriptor pipeline_descriptor;
  NyblerRenderPassColorAttachment attachment;
  NyblerRenderPassDescriptor pass_descriptor;
  NyblerBufferDescriptor buffer_descriptor;
  NyblerTextureDescriptor texture_descriptor;
  NyblerSamplerDescriptor sampler_descriptor;
  NyblerExtent3D extent = {64, 64, 1};

  NyblerInstance *instance = NULL;
  NyblerAdapter *adapter = NULL;
  NyblerDevice *device = NULL;
  NyblerQueue *queue = NULL;
  NyblerBuffer *vertices = NULL;
  NyblerBuffer *uniform = NULL;
  NyblerTexture *texture = NULL;
  NyblerSampler *sampler = NULL;
  NyblerBindGroupLayout *layout = NULL;
  NyblerBindGroup *group = NULL;
  NyblerShaderModule *vertex_module = NULL;
  NyblerShaderModule *fragment_module = NULL;
  NyblerRenderPipeline *pipeline = NULL;
  NyblerCommandEncoder *encoder = NULL;
  NyblerRenderPassEncoder *pass = NULL;
  NyblerRenderPassEncoder *nested = NULL;
  NyblerCommandBuffer *commands = NULL;
  float readback[4] = {0};
  int ok = 1;

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
  buffer_descriptor.size = sizeof(initial_vertices);
  buffer_descriptor.usage = NYBLER_BUFFER_USAGE_VERTEX | NYBLER_BUFFER_USAGE_COPY_DST;
  buffer_descriptor.initial_data = initial_vertices;
  buffer_descriptor.label = "vertices";
  ok = check(
    nybler_device_create_buffer(device, &buffer_descriptor, &vertices) == NYBLER_OK,
    "create vertex buffer"
  ) && ok;
  ok = check(
    nybler_buffer_read(vertices, 0, readback, sizeof(readback)) == NYBLER_OK &&
    memcmp(readback, initial_vertices, sizeof(readback)) == 0,
    "read initial vertex data"
  ) && ok;

  memset(&buffer_descriptor, 0, sizeof(buffer_descriptor));
  buffer_descriptor.size = sizeof(uniform_value);
  buffer_descriptor.usage = NYBLER_BUFFER_USAGE_UNIFORM;
  ok = check(
    nybler_device_create_buffer(device, &buffer_descriptor, &uniform) == NYBLER_OK,
    "create uniform buffer"
  ) && ok;
  ok = check(
    nybler_buffer_write(uniform, 0, &uniform_value, sizeof(uniform_value)) == NYBLER_OK,
    "write uniform data"
  ) && ok;
  ok = check(
    nybler_buffer_write(uniform, 8, &uniform_value, sizeof(uniform_value)) != NYBLER_OK,
    "reject out-of-bounds write"
  ) && ok;

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size = extent;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage = NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT;
  ok = check(
    nybler_device_create_texture(device, &texture_descriptor, &texture) == NYBLER_OK,
    "create render texture"
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

  ok = check(
    nybler_device_create_bind_group_layout(
      device, &layout_descriptor, &layout) == NYBLER_OK,
    "create bind group layout"
  ) && ok;
  memset(&bind_entry, 0, sizeof(bind_entry));
  bind_entry.binding = 0;
  bind_entry.buffer = uniform;
  bind_descriptor.layout = layout;
  bind_descriptor.entries = &bind_entry;
  bind_descriptor.entry_count = 1;
  ok = check(
    nybler_device_create_bind_group(device, &bind_descriptor, &group) == NYBLER_OK,
    "create bind group"
  ) && ok;

  ok = check(
    nybler_shader_module_create(
      device, NYBLER_SHADER_FORMAT_WGSL, shader_source,
      sizeof(shader_source) - 1, "vs", &vertex_module) == NYBLER_OK,
    "create vertex shader module"
  ) && ok;
  ok = check(
    nybler_shader_module_create(
      device, NYBLER_SHADER_FORMAT_WGSL, shader_source,
      sizeof(shader_source) - 1, "fs", &fragment_module) == NYBLER_OK,
    "create fragment shader module"
  ) && ok;

  memset(&pipeline_descriptor, 0, sizeof(pipeline_descriptor));
  pipeline_descriptor.vertex_module = vertex_module;
  pipeline_descriptor.fragment_module = fragment_module;
  pipeline_descriptor.vertex_entry_point = "vs_main";
  pipeline_descriptor.fragment_entry_point = "fs_main";
  pipeline_descriptor.targets = &target;
  pipeline_descriptor.target_count = 1;
  pipeline_descriptor.topology = NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_descriptor.layout = layout;
  ok = check(
    nybler_device_create_render_pipeline(
      device, &pipeline_descriptor, &pipeline) == NYBLER_OK,
    "create render pipeline"
  ) && ok;

  ok = check(
    nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
    "create command encoder"
  ) && ok;

  memset(&attachment, 0, sizeof(attachment));
  attachment.texture = texture;
  attachment.clear_value = (NyblerColor){1.0f, 0.5f, 0.0f, 1.0f};
  attachment.load_op = NYBLER_LOAD_OP_CLEAR;
  attachment.store_op = NYBLER_STORE_OP_STORE;
  pass_descriptor.color_attachments = &attachment;
  pass_descriptor.color_attachment_count = 1;
  ok = check(
    nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &pass) == NYBLER_OK,
    "begin render pass"
  ) && ok;
  ok = check(
    nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &nested) != NYBLER_OK,
    "reject nested render pass"
  ) && ok;

  nybler_render_pass_set_pipeline(pass, pipeline);
  nybler_render_pass_set_bind_group(pass, 0, group);
  nybler_render_pass_set_vertex_buffer(pass, 0, vertices, 0);
  nybler_render_pass_draw(pass, 3, 1, 0, 0);
  nybler_render_pass_end(pass);

  ok = check(
    nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
    "finish command buffer"
  ) && ok;
  ok = check(
    nybler_queue_submit(queue, 1, &commands) == NYBLER_OK,
    "submit consumes command buffer"
  ) && ok;
  commands = NULL; /* nybler_queue_submit() takes ownership. */

  printf("nybler resources smoke: buffer/texture/sampler/pipeline/pass/queue ok\n");

  nybler_command_encoder_release(encoder);
  nybler_render_pipeline_release(pipeline);
  nybler_shader_module_release(fragment_module);
  nybler_shader_module_release(vertex_module);
  nybler_bind_group_release(group);
  nybler_bind_group_layout_release(layout);
  nybler_sampler_release(sampler);
  nybler_texture_release(texture);
  nybler_buffer_release(uniform);
  nybler_buffer_release(vertices);
  nybler_device_release(device);
  nybler_adapter_release(adapter);
  nybler_shutdown(instance);
  return ok ? 0 : 1;
}
