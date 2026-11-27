#include "nybler/nybler.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "mesa smoke failed: %s\n", message);
  return 0;
}

int main(void) {
  static const char vertex_wgsl[] =
    "@vertex\n"
    "fn vs_main(@location(0) position: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
    "  return position;\n"
    "}\n";
  static const char fragment_wgsl[] =
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4<f32> {\n"
    "  return vec4<f32>(1.0, 0.0, 1.0, 1.0);\n"
    "}\n";
  static const float triangle[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     3.0f, -1.0f, 0.0f, 1.0f,
    -1.0f,  3.0f, 0.0f, 1.0f
  };

  NyblerInitOptions options = {1, NYBLER_BACKEND_MESA_GL, 1, NULL};
  NyblerInstance *instance = NULL;
  NyblerAdapter *adapter = NULL;
  NyblerDevice *device = NULL;
  NyblerQueue *queue = NULL;
  NyblerTexture *texture = NULL;
  NyblerBuffer *readback = NULL;
  NyblerBuffer *vertices = NULL;
  NyblerShaderModule *vertex_module = NULL;
  NyblerShaderModule *fragment_module = NULL;
  NyblerRenderPipeline *pipeline = NULL;
  NyblerCommandEncoder *encoder = NULL;
  NyblerRenderPassEncoder *pass = NULL;
  NyblerCommandBuffer *commands = NULL;
  NyblerColorTargetState target = {NYBLER_TEXTURE_FORMAT_RGBA8_UNORM, 0};
  NyblerTextureDescriptor texture_descriptor;
  NyblerBufferDescriptor readback_descriptor;
  NyblerBufferDescriptor vertex_descriptor;
  NyblerRenderPassColorAttachment attachment;
  NyblerRenderPassDescriptor pass_descriptor;
  NyblerRenderPipelineDescriptor pipeline_descriptor;
  NyblerImageCopyTexture copy_source;
  NyblerImageCopyBuffer copy_destination;
  NyblerExtent3D copy_size = {4, 4, 1};
  NyblerCommandBuffer *submission[1];
  uint8_t pixels[64];
  int ok = 1;

  if (nybler_init(&options, &instance) != NYBLER_OK ||
      nybler_instance_request_adapter(instance, &adapter) != NYBLER_OK) {
    fprintf(stderr, "mesa smoke skipped: instance/adapter unavailable\n");
    nybler_adapter_release(adapter);
    nybler_shutdown(instance);
    return 0;
  }
  if (nybler_adapter_request_device(adapter, &device) != NYBLER_OK) {
    fprintf(stderr, "mesa smoke skipped: local Mesa GL backend unavailable\n");
    nybler_adapter_release(adapter);
    nybler_shutdown(instance);
    return 0;
  }
  queue = nybler_device_get_queue(device);

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size.width = 4;
  texture_descriptor.size.height = 4;
  texture_descriptor.size.depth_or_array_layers = 1;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage =
    NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT | NYBLER_TEXTURE_USAGE_COPY_SRC;
  ok = check(
    nybler_device_create_texture(device, &texture_descriptor, &texture) == NYBLER_OK,
    "create texture"
  ) && ok;

  memset(&readback_descriptor, 0, sizeof(readback_descriptor));
  readback_descriptor.size = sizeof(pixels);
  readback_descriptor.usage = NYBLER_BUFFER_USAGE_COPY_DST;
  ok = check(
    nybler_device_create_buffer(device, &readback_descriptor, &readback) == NYBLER_OK,
    "create readback buffer"
  ) && ok;

  memset(&attachment, 0, sizeof(attachment));
  attachment.texture = texture;
  attachment.clear_value.r = 1.0f;
  attachment.clear_value.a = 1.0f;
  attachment.load_op = NYBLER_LOAD_OP_CLEAR;
  attachment.store_op = NYBLER_STORE_OP_STORE;
  memset(&pass_descriptor, 0, sizeof(pass_descriptor));
  pass_descriptor.color_attachments = &attachment;
  pass_descriptor.color_attachment_count = 1;

  if (ok) {
    ok = check(
      nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
      "create clear encoder"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &pass) ==
        NYBLER_OK,
      "begin clear pass"
    ) && ok;
  }
  if (ok) {
    nybler_render_pass_end(pass);
    pass = NULL;
    ok = check(
      nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
      "finish clear encoder"
    ) && ok;
    submission[0] = commands;
    ok = check(
      nybler_queue_submit(queue, 1, submission) == NYBLER_OK,
      "submit clear"
    ) && ok;
    commands = NULL;
  }
  nybler_command_encoder_release(encoder);
  encoder = NULL;

  if (ok) {
    memset(&copy_source, 0, sizeof(copy_source));
    copy_source.texture = texture;
    memset(&copy_destination, 0, sizeof(copy_destination));
    copy_destination.buffer = readback;
    copy_destination.bytes_per_row = 16;
    ok = check(
      nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
      "create copy encoder"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_copy_texture_to_buffer(
        encoder, &copy_source, &copy_destination, &copy_size) == NYBLER_OK,
      "record clear copy"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
      "finish copy encoder"
    ) && ok;
    submission[0] = commands;
    ok = check(
      nybler_queue_submit(queue, 1, submission) == NYBLER_OK,
      "submit clear copy"
    ) && ok;
    commands = NULL;
  }
  nybler_command_encoder_release(encoder);
  encoder = NULL;

  if (ok) {
    ok = check(
      nybler_buffer_read(readback, 0, pixels, sizeof(pixels)) == NYBLER_OK &&
        pixels[0] == 255 && pixels[1] == 0 && pixels[2] == 0 &&
        pixels[3] == 255,
      "clear pixel"
    ) && ok;
  }

  memset(&vertex_descriptor, 0, sizeof(vertex_descriptor));
  vertex_descriptor.size = sizeof(triangle);
  vertex_descriptor.usage = NYBLER_BUFFER_USAGE_VERTEX;
  vertex_descriptor.initial_data = triangle;
  if (ok) {
    ok = check(
      nybler_device_create_buffer(device, &vertex_descriptor, &vertices) == NYBLER_OK,
      "create vertex buffer"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_shader_module_create(
        device,
        NYBLER_SHADER_FORMAT_WGSL,
        vertex_wgsl,
        sizeof(vertex_wgsl) - 1,
        NULL,
        &vertex_module
      ) == NYBLER_OK,
      "create vertex shader"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_shader_module_create(
        device,
        NYBLER_SHADER_FORMAT_WGSL,
        fragment_wgsl,
        sizeof(fragment_wgsl) - 1,
        NULL,
        &fragment_module
      ) == NYBLER_OK,
      "create fragment shader"
    ) && ok;
  }

  memset(&pipeline_descriptor, 0, sizeof(pipeline_descriptor));
  pipeline_descriptor.vertex_module = vertex_module;
  pipeline_descriptor.fragment_module = fragment_module;
  pipeline_descriptor.vertex_entry_point = "vs_main";
  pipeline_descriptor.fragment_entry_point = "fs_main";
  pipeline_descriptor.targets = &target;
  pipeline_descriptor.target_count = 1;
  pipeline_descriptor.topology = NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  if (ok) {
    ok = check(
      nybler_device_create_render_pipeline(
        device, &pipeline_descriptor, &pipeline) == NYBLER_OK,
      "create render pipeline"
    ) && ok;
  }

  if (ok) {
    attachment.clear_value.r = 0.0f;
    attachment.clear_value.g = 0.0f;
    attachment.clear_value.b = 1.0f;
    ok = check(
      nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
      "create draw encoder"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_begin_render_pass(encoder, &pass_descriptor, &pass) ==
        NYBLER_OK,
      "begin draw pass"
    ) && ok;
  }
  if (ok) {
    nybler_render_pass_set_pipeline(pass, pipeline);
    nybler_render_pass_set_vertex_buffer(pass, 0, vertices, 0);
    nybler_render_pass_draw(pass, 3, 1, 0, 0);
    nybler_render_pass_end(pass);
    pass = NULL;
    ok = check(
      nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
      "finish draw encoder"
    ) && ok;
  }
  if (ok) {
    submission[0] = commands;
    ok = check(
      nybler_queue_submit(queue, 1, submission) == NYBLER_OK,
      "submit draw"
    ) && ok;
    commands = NULL;
  }
  nybler_command_encoder_release(encoder);
  encoder = NULL;

  if (ok) {
    memset(&copy_source, 0, sizeof(copy_source));
    copy_source.texture = texture;
    memset(&copy_destination, 0, sizeof(copy_destination));
    copy_destination.buffer = readback;
    copy_destination.bytes_per_row = 16;
    ok = check(
      nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
      "create draw copy encoder"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_copy_texture_to_buffer(
        encoder, &copy_source, &copy_destination, &copy_size) == NYBLER_OK,
      "record draw copy"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
      "finish draw copy encoder"
    ) && ok;
  }
  if (ok) {
    submission[0] = commands;
    ok = check(
      nybler_queue_submit(queue, 1, submission) == NYBLER_OK,
      "submit draw copy"
    ) && ok;
    commands = NULL;
  }
  nybler_command_encoder_release(encoder);
  encoder = NULL;

  if (ok) {
    ok = check(
      nybler_buffer_read(readback, 0, pixels, sizeof(pixels)) == NYBLER_OK &&
        pixels[8 * 4 + 2 * 4 + 0] == 255 &&
        pixels[8 * 4 + 2 * 4 + 1] == 0 &&
        pixels[8 * 4 + 2 * 4 + 2] == 255,
      "triangle pixel"
    ) && ok;
  }

  nybler_command_buffer_release(commands);
  nybler_render_pipeline_release(pipeline);
  nybler_shader_module_release(fragment_module);
  nybler_shader_module_release(vertex_module);
  nybler_buffer_release(vertices);
  nybler_buffer_release(readback);
  nybler_texture_release(texture);
  nybler_device_release(device);
  nybler_adapter_release(adapter);
  nybler_shutdown(instance);

  if (ok) printf("nybler mesa smoke: clear/triangle/readback ok\n");
  return ok ? 0 : 1;
}
