#include "nybler/nybler.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "paint smoke failed: %s\n", message);
  return 0;
}

static size_t count_color(
  const uint8_t *pixels,
  size_t pixel_count,
  uint8_t r,
  uint8_t g,
  uint8_t b
) {
  size_t count = 0;
  size_t i;

  for (i = 0; i < pixel_count; ++i) {
    const uint8_t *pixel = &pixels[i * 4];
    if (pixel[0] == r && pixel[1] == g && pixel[2] == b) count++;
  }
  return count;
}

int main(void) {
  static const char html[] =
    "<style>"
    "body { width: 128px; height: 128px; background-color: #0000ff; }"
    "#panel { width: 64px; height: 64px; background-color: #ff0000; }"
    "h1 { color: #00ff00; }"
    "</style>"
    "<div id='panel'><h1>hi</h1></div>";
  static const size_t width = 128;
  static const size_t height = 128;

  NyblerInitOptions options = {1, NYBLER_BACKEND_MESA_GL, 1, NULL};
  NyblerViewport viewport = {(float)width, (float)height};
  NyblerDocument *document = NULL;
  NyblerLayout *layout = NULL;
  NyblerDisplayList *display_list = NULL;
  NyblerInstance *instance = NULL;
  NyblerAdapter *adapter = NULL;
  NyblerDevice *device = NULL;
  NyblerQueue *queue = NULL;
  NyblerTexture *target = NULL;
  NyblerBuffer *readback = NULL;
  NyblerPainter *painter = NULL;
  NyblerCommandEncoder *encoder = NULL;
  NyblerCommandBuffer *commands = NULL;
  NyblerTextureDescriptor texture_descriptor;
  NyblerBufferDescriptor readback_descriptor;
  NyblerRenderPassColorAttachment attachment;
  NyblerRenderPassDescriptor render_pass;
  NyblerImageCopyTexture source;
  NyblerImageCopyBuffer destination;
  NyblerExtent3D copy_size = {
    (uint32_t)width, (uint32_t)height, 1
  };
  NyblerCommandBuffer *submission[1];
  uint8_t *pixels = NULL;
  size_t green_count = 0;
  size_t red_count = 0;
  size_t blue_count = 0;
  int ok = 1;

  if (nybler_document_parse(html, sizeof(html) - 1, &document) != NYBLER_OK ||
      nybler_layout_compute(document, &viewport, &layout) != NYBLER_OK ||
      nybler_layout_display_list(layout, &display_list) != NYBLER_OK) {
    fprintf(stderr, "paint smoke failed: build display list\n");
    ok = 0;
    goto done;
  }

  if (nybler_init(&options, &instance) != NYBLER_OK ||
      nybler_instance_request_adapter(instance, &adapter) != NYBLER_OK ||
      nybler_adapter_request_device(adapter, &device) != NYBLER_OK) {
    fprintf(stderr, "paint smoke skipped: local Mesa GL backend unavailable\n");
    goto done;
  }
  queue = nybler_device_get_queue(device);

  memset(&texture_descriptor, 0, sizeof(texture_descriptor));
  texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_descriptor.size.width = (uint32_t)width;
  texture_descriptor.size.height = (uint32_t)height;
  texture_descriptor.size.depth_or_array_layers = 1;
  texture_descriptor.mip_level_count = 1;
  texture_descriptor.sample_count = 1;
  texture_descriptor.usage =
    NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT | NYBLER_TEXTURE_USAGE_COPY_SRC;
  ok = check(
    nybler_device_create_texture(device, &texture_descriptor, &target) == NYBLER_OK,
    "create target texture"
  ) && ok;

  pixels = (uint8_t *)malloc(width * height * 4);
  if (!pixels) {
    fprintf(stderr, "paint smoke failed: allocate readback\n");
    ok = 0;
    goto done;
  }
  memset(&readback_descriptor, 0, sizeof(readback_descriptor));
  readback_descriptor.size = (uint64_t)width * height * 4;
  readback_descriptor.usage = NYBLER_BUFFER_USAGE_COPY_DST;
  ok = check(
    nybler_device_create_buffer(device, &readback_descriptor, &readback) == NYBLER_OK,
    "create readback buffer"
  ) && ok;

  ok = check(
    nybler_painter_create(device, &painter) == NYBLER_OK,
    "create painter"
  ) && ok;
  ok = check(
    nybler_device_create_command_encoder(device, &encoder) == NYBLER_OK,
    "create paint encoder"
  ) && ok;

  memset(&attachment, 0, sizeof(attachment));
  attachment.texture = target;
  attachment.clear_value.a = 1.0f;
  attachment.load_op = NYBLER_LOAD_OP_CLEAR;
  attachment.store_op = NYBLER_STORE_OP_STORE;
  memset(&render_pass, 0, sizeof(render_pass));
  render_pass.color_attachments = &attachment;
  render_pass.color_attachment_count = 1;
  render_pass.label = "paint display list";

  if (ok) {
    ok = check(
      nybler_painter_record(
        painter, encoder, &viewport, display_list, &render_pass) == NYBLER_OK,
      "record display list"
    ) && ok;
  }
  if (ok) {
    memset(&source, 0, sizeof(source));
    source.texture = target;
    memset(&destination, 0, sizeof(destination));
    destination.buffer = readback;
    destination.bytes_per_row = (uint32_t)width * 4;
    ok = check(
      nybler_command_encoder_copy_texture_to_buffer(
        encoder, &source, &destination, &copy_size) == NYBLER_OK,
      "record readback copy"
    ) && ok;
  }
  if (ok) {
    ok = check(
      nybler_command_encoder_finish(encoder, &commands) == NYBLER_OK,
      "finish paint encoder"
    ) && ok;
  }
  if (ok) {
    submission[0] = commands;
    ok = check(
      nybler_queue_submit(queue, 1, submission) == NYBLER_OK,
      "submit paint commands"
    ) && ok;
    commands = NULL;
  }
  if (ok) {
    ok = check(
      nybler_buffer_read(readback, 0, pixels, readback_descriptor.size) == NYBLER_OK,
      "read painted pixels"
    ) && ok;
  }
  if (ok) {
    green_count = count_color(pixels, width * height, 0, 255, 0);
    red_count = count_color(pixels, width * height, 255, 0, 0);
    blue_count = count_color(pixels, width * height, 0, 0, 255);
    ok = check(green_count > 0, "text color") && ok;
    ok = check(red_count > 0, "box color") && ok;
    ok = check(blue_count > 0, "body color") && ok;
  }

done:
  free(pixels);
  nybler_command_buffer_release(commands);
  nybler_command_encoder_release(encoder);
  nybler_painter_release(painter);
  nybler_buffer_release(readback);
  nybler_texture_release(target);
  nybler_device_release(device);
  nybler_adapter_release(adapter);
  nybler_shutdown(instance);
  nybler_display_list_release(display_list);
  nybler_layout_release(layout);
  nybler_document_release(document);

  if (ok) {
    printf(
      "nybler paint smoke: DOM -> layout -> display list -> GPU ok "
      "(%zu green, %zu red, %zu blue pixels)\n",
      green_count, red_count, blue_count);
  }
  return ok ? 0 : 1;
}
