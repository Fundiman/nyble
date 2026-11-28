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

    const nybler::NyColor clear_color{1.0f, 0.0f, 0.0f, 1.0f};
    NyblerRenderPassColorAttachment attachment{};
    attachment.texture = texture.native_handle();
    attachment.clear_value = clear_color;
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

    nybler::NyPicture picture;
    ok = check(
      nybler::create_picture(&picture) == NYBLER_OK && picture.valid(),
      "create empty picture"
    ) && ok;
    const nybler::NyRect box{0.0f, 0.0f, 1.0f, 1.0f};
    const nybler::NyPaint paint{clear_color};
    ok = check(picture.append_rect(box, paint) == NYBLER_OK, "append rect") && ok;
    ok = check(picture.append_text("Ny", box, paint) == NYBLER_OK, "append text") && ok;
    ok = check(picture.length() == 2, "picture length") && ok;
    ok = check(
      picture.item(0) && picture.item(0)->kind == NYBLER_DISPLAY_ITEM_BOX,
      "picture rect item"
    ) && ok;
    ok = check(
      picture.item(1) && picture.item(1)->kind == NYBLER_DISPLAY_ITEM_TEXT,
      "picture text item"
    ) && ok;

    nybler::NyCanvas canvas;
    ok = check(
      nybler::create_canvas(device, &canvas) == NYBLER_OK,
      "create canvas wrapper"
    ) && ok;

    nybler::CommandEncoder draw_encoder;
    nybler::CommandBuffer draw_commands;
    const nybler::NyViewport viewport{width, height};

    ok = check(
      nybler::create_command_encoder(device, &draw_encoder) == NYBLER_OK,
      "create draw encoder"
    ) && ok;
    ok = check(
      canvas.draw_picture(picture, draw_encoder, viewport, pass_descriptor) == NYBLER_OK,
      "draw picture"
    ) && ok;
    ok = check(
      draw_encoder.finish(draw_commands) == NYBLER_OK,
      "finish draw commands"
    ) && ok;
    ok = check(queue.submit(draw_commands) == NYBLER_OK, "submit draw commands") && ok;

    ok = check(
      nybler::create_command_encoder(device, &draw_encoder) == NYBLER_OK,
      "create rect encoder"
    ) && ok;
    ok = check(
      canvas.draw_rect(box, paint, draw_encoder, viewport, pass_descriptor) == NYBLER_OK,
      "draw rect"
    ) && ok;
    ok = check(
      draw_encoder.finish(draw_commands) == NYBLER_OK,
      "finish rect commands"
    ) && ok;
    ok = check(queue.submit(draw_commands) == NYBLER_OK, "submit rect commands") && ok;

    if (argc > 1) {
      nybler::NyFont font;
      ok = check(
        nybler::create_font(device, argv[1], 16.0f, &font) == NYBLER_OK,
        "create font wrapper"
      ) && ok;
      if (font.valid()) {
        ok = check(
          nybler::create_command_encoder(device, &draw_encoder) == NYBLER_OK,
          "create text encoder"
        ) && ok;
        ok = check(
          canvas.draw_text(
            "Ny", box, paint, font, draw_encoder, viewport, pass_descriptor) == NYBLER_OK,
          "draw text"
        ) && ok;
        ok = check(
          draw_encoder.finish(draw_commands) == NYBLER_OK,
          "finish text commands"
        ) && ok;
        ok = check(queue.submit(draw_commands) == NYBLER_OK, "submit text commands") && ok;
        canvas.clear_font();
      }
    }
  }

  nybler_device_release(device);
  nybler_adapter_release(adapter);

  if (ok) {
    std::puts(
      "nybler cpp api smoke: buffer/NyCanvas/NyFont wrappers, picture drawing, copy/readback ok");
  }
  return ok ? 0 : 1;
}
