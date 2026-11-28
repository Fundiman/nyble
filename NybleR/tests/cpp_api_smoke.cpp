#include "nybler/nybler.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int check(bool condition, const char *message) {
  if (condition) return 1;
  std::fprintf(stderr, "cpp api smoke failed: %s\n", message);
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  static const std::uint32_t width = 4;
  static const std::uint32_t height = 4;
  static const std::uint32_t bytes_per_row = width * 4;
  static const std::size_t pixel_size = bytes_per_row * height;
  static const std::uint8_t texture_pixels[] = {
    255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
    255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
    255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255,
    255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255
  };

  nybler::Instance instance;
  NyblerAdapter *adapter = nullptr;
  NyblerDevice *device = nullptr;
  int ok = 1;

  if (nybler::init(nybler::Backend::MesaGL, true, nullptr, &instance) != NYBLER_OK ||
      nybler_instance_request_adapter(instance.native_handle(), &adapter) != NYBLER_OK) {
    std::fprintf(stderr, "cpp api smoke skipped: instance/adapter unavailable\n");
    nybler_adapter_release(adapter);
    return 0;
  }
  if (nybler_adapter_request_device(adapter, &device) != NYBLER_OK) {
    std::fprintf(stderr, "cpp api smoke skipped: local Mesa GL backend unavailable\n");
    nybler_adapter_release(adapter);
    return 0;
  }

  {
    NyblerTextureDescriptor texture_descriptor{};
    texture_descriptor.format = NYBLER_TEXTURE_FORMAT_RGBA8_UNORM;
    texture_descriptor.size = {width, height, 1};
    texture_descriptor.mip_level_count = 1;
    texture_descriptor.sample_count = 1;
    texture_descriptor.usage =
      NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT | NYBLER_TEXTURE_USAGE_COPY_SRC;
    texture_descriptor.initial_data = texture_pixels;

    NyblerBufferDescriptor buffer_descriptor{};
    buffer_descriptor.size = pixel_size;
    buffer_descriptor.usage = NYBLER_BUFFER_USAGE_COPY_DST;

    nybler::Texture texture;
    nybler::Buffer readback;
    nybler::Buffer values;
    nybler::CommandEncoder encoder;
    nybler::CommandBuffer commands;
    nybler::Queue queue = nybler::device_queue(device);

    ok = check(
      nybler::create_texture(device, texture_descriptor, &texture) == NYBLER_OK,
      "create texture wrapper"
    ) && ok;
    ok = check(
      nybler::create_buffer(device, buffer_descriptor, &readback) == NYBLER_OK,
      "create readback wrapper"
    ) && ok;
    buffer_descriptor.size = 8;
    ok = check(
      nybler::create_buffer(device, buffer_descriptor, &values) == NYBLER_OK,
      "create value buffer wrapper"
    ) && ok;

    const float written_float = 1.25f;
    float read_float = 0.0f;
    ok = check(values.write(0, written_float) == NYBLER_OK, "typed buffer write") && ok;
    ok = check(values.read(0, read_float) == NYBLER_OK, "typed buffer read") && ok;
    ok = check(read_float == written_float, "typed buffer round trip") && ok;

    const std::vector<std::uint8_t> written_bytes = {1, 2, 3, 4};
    std::vector<std::uint8_t> read_bytes;
    ok = check(values.write(4, written_bytes) == NYBLER_OK, "byte buffer write") && ok;
    ok = check(values.read(4, written_bytes.size(), read_bytes) == NYBLER_OK,
               "byte buffer read") && ok;
    ok = check(read_bytes == written_bytes, "byte buffer round trip") && ok;

    NyblerRenderPassColorAttachment attachment{};
    attachment.texture = texture.native_handle();
    attachment.clear_value = {1.0f, 0.0f, 0.0f, 1.0f};
    attachment.load_op = NYBLER_LOAD_OP_CLEAR;
    attachment.store_op = NYBLER_STORE_OP_STORE;

    NyblerRenderPassDescriptor pass_descriptor{};
    pass_descriptor.color_attachments = &attachment;
    pass_descriptor.color_attachment_count = 1;
    pass_descriptor.label = "cpp api clear";

    ok = check(
      nybler::create_command_encoder(device, &encoder) == NYBLER_OK,
      "create command encoder wrapper"
    ) && ok;

    nybler::RenderPassEncoder pass;
    ok = check(
      encoder.begin_render_pass(pass_descriptor, pass) == NYBLER_OK,
      "begin render pass wrapper"
    ) && ok;
    pass.end();
    ok = check(!pass.valid(), "render pass ends once") && ok;

    ok = check(
      encoder.copy_texture_to_buffer(
        texture, readback, {width, height, 1}, bytes_per_row) == NYBLER_OK,
      "copy texture to buffer wrapper"
    ) && ok;
    ok = check(encoder.finish(commands) == NYBLER_OK, "finish command buffer wrapper") && ok;
    ok = check(queue.submit(commands) == NYBLER_OK, "submit command buffer wrapper") && ok;
    ok = check(!commands.valid(), "submitted command buffer is consumed") && ok;

    std::vector<std::uint8_t> pixels;
    ok = check(
      readback.read(0, pixel_size, pixels) == NYBLER_OK,
      "read pixel wrapper"
    ) && ok;
    ok = check(
      pixels.size() == sizeof(texture_pixels) &&
        std::memcmp(pixels.data(), texture_pixels, sizeof(texture_pixels)) == 0,
      "readback matches cleared texture"
    ) && ok;

    if (argc > 1) {
      nybler::TextRenderer text_renderer;
      nybler::Painter painter;
      ok = check(
        nybler::create_text_renderer(device, argv[1], 16.0f, &text_renderer) == NYBLER_OK,
        "create text renderer wrapper"
      ) && ok;
      ok = check(
        nybler::create_painter(device, &painter) == NYBLER_OK,
        "create painter wrapper"
      ) && ok;
      if (text_renderer.valid() && painter.valid()) {
        painter.set_text_renderer(text_renderer);
        painter.clear_text_renderer();
      }
    }
  }

  nybler_device_release(device);
  nybler_adapter_release(adapter);

  if (ok) {
    std::puts("nybler cpp api smoke: buffer/text/painter wrappers, copy/readback ok");
  }
  return ok ? 0 : 1;
}
