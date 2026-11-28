#ifndef NYBLER_NYBLER_HPP
#define NYBLER_NYBLER_HPP

#include "nybler/nybler.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

namespace nybler {

enum class Backend : std::uint32_t {
  Auto = NYBLER_BACKEND_AUTO,
  MesaVulkan = NYBLER_BACKEND_MESA_VULKAN,
  MesaGL = NYBLER_BACKEND_MESA_GL,
  Null = NYBLER_BACKEND_NULL,
};

enum class ShaderFormat : std::uint32_t {
  Unknown = NYBLER_SHADER_FORMAT_UNKNOWN,
  WGSL = NYBLER_SHADER_FORMAT_WGSL,
  SPIRV = NYBLER_SHADER_FORMAT_SPIRV,
  GLSL = NYBLER_SHADER_FORMAT_GLSL,
  MSL = NYBLER_SHADER_FORMAT_MSL,
  HLSL = NYBLER_SHADER_FORMAT_HLSL,
};

struct Version {
  std::uint32_t number;
  std::string text;
};

inline Version version() {
  return {nybler_version_number(), nybler_version_string()};
}

namespace detail {

/* Owned handles mirror the C ABI's release function. The stable ABI stays C;
 * these wrappers only provide RAII and typed convenience methods. */
template <typename T, void (*Release)(T *)>
class OwnedHandle {
 public:
  OwnedHandle() = default;
  explicit OwnedHandle(T *handle) noexcept : handle_(handle) {}

  ~OwnedHandle() { reset(); }

  OwnedHandle(const OwnedHandle &) = delete;
  OwnedHandle &operator=(const OwnedHandle &) = delete;

  OwnedHandle(OwnedHandle &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  OwnedHandle &operator=(OwnedHandle &&other) noexcept {
    if (this != &other) {
      reset(other.handle_);
      other.handle_ = nullptr;
    }
    return *this;
  }

  [[nodiscard]] T *native_handle() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }

  void reset(T *handle = nullptr) noexcept {
    if (handle_ == handle) return;
    if (handle_) Release(handle_);
    handle_ = handle;
  }

  /* Relinquish ownership. Needed for C calls such as nybler_queue_submit()
   * that consume the underlying handle. */
  T *release() noexcept {
    T *handle = handle_;
    handle_ = nullptr;
    return handle;
  }

 protected:
  T *handle_ = nullptr;
};

/* Queues and render pass encoders are borrowed handles. They are movable but
 * never release the underlying object. */
template <typename T>
class BorrowedHandle {
 public:
  BorrowedHandle() = default;
  explicit BorrowedHandle(T *handle) noexcept : handle_(handle) {}
  ~BorrowedHandle() = default;

  BorrowedHandle(const BorrowedHandle &) = delete;
  BorrowedHandle &operator=(const BorrowedHandle &) = delete;

  BorrowedHandle(BorrowedHandle &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  BorrowedHandle &operator=(BorrowedHandle &&other) noexcept {
    if (this != &other) {
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  [[nodiscard]] T *native_handle() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }

  void reset(T *handle = nullptr) noexcept { handle_ = handle; }

 protected:
  T *handle_ = nullptr;
};

}  // namespace detail

class Instance {
 public:
  Instance() = default;
  Instance(const Instance &) = delete;
  Instance &operator=(const Instance &) = delete;

  explicit Instance(NyblerInstance *handle) : handle_(handle) {}

  ~Instance() { reset(); }

  Instance(Instance &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Instance &operator=(Instance &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  [[nodiscard]] NyblerInstance *native_handle() const { return handle_; }
  [[nodiscard]] bool valid() const { return handle_ != nullptr; }

  void reset() {
    if (handle_) {
      nybler_shutdown(handle_);
      handle_ = nullptr;
    }
  }

 private:
  NyblerInstance *handle_ = nullptr;
};

inline NyblerResult init(
  Backend backend,
  bool headless,
  const char *preferred_adapter,
  Instance *out_instance
) {
  if (!out_instance) return NYBLER_ERROR_INVALID_ARGUMENT;

  NyblerInitOptions options{};
  options.version = 1;
  options.backend = static_cast<NyblerBackend>(backend);
  options.headless = headless ? 1 : 0;
  options.preferred_adapter = preferred_adapter;

  NyblerInstance *raw = nullptr;
  const NyblerResult result = nybler_init(&options, &raw);
  if (result == NYBLER_OK) *out_instance = Instance(raw);
  return result;
}

/* Resource wrappers borrow their device. Release them before the device. */
class Buffer : public detail::OwnedHandle<NyblerBuffer, nybler_buffer_release> {
 public:
  using OwnedHandle::OwnedHandle;

  NyblerResult write(std::uint64_t offset, const void *data, std::size_t size) {
    return nybler_buffer_write(handle_, offset, data, size);
  }

  NyblerResult read(std::uint64_t offset, void *data, std::size_t size) const {
    return nybler_buffer_read(handle_, offset, data, size);
  }

  template <typename T>
  NyblerResult write(std::uint64_t offset, const T &value) {
    static_assert(
      std::is_trivially_copyable<T>::value,
      "nybler::Buffer::write() requires a trivially copyable value");
    return write(offset, &value, sizeof(value));
  }

  template <typename T>
  NyblerResult read(std::uint64_t offset, T &value) const {
    static_assert(
      std::is_trivially_copyable<T>::value,
      "nybler::Buffer::read() requires a trivially copyable value");
    return read(offset, &value, sizeof(value));
  }

  NyblerResult write(std::uint64_t offset, const std::vector<std::uint8_t> &bytes) {
    return write(offset, bytes.data(), bytes.size());
  }

  NyblerResult read(
    std::uint64_t offset,
    std::size_t size,
    std::vector<std::uint8_t> &bytes
  ) const {
    bytes.resize(size);
    return read(offset, bytes.data(), size);
  }
};

class Texture : public detail::OwnedHandle<NyblerTexture, nybler_texture_release> {
 public:
  using OwnedHandle::OwnedHandle;
};

class CommandBuffer
  : public detail::OwnedHandle<NyblerCommandBuffer, nybler_command_buffer_release> {
 public:
  using OwnedHandle::OwnedHandle;
};

class RenderPassEncoder : public detail::BorrowedHandle<NyblerRenderPassEncoder> {
 public:
  using BorrowedHandle::BorrowedHandle;

  ~RenderPassEncoder() { end(); }

  void end() {
    if (!handle_) return;
    nybler_render_pass_end(handle_);
    handle_ = nullptr;
  }

  void set_pipeline(NyblerRenderPipeline *pipeline) {
    nybler_render_pass_set_pipeline(handle_, pipeline);
  }

  void set_bind_group(std::uint32_t index, NyblerBindGroup *group) {
    nybler_render_pass_set_bind_group(handle_, index, group);
  }

  void set_vertex_buffer(std::uint32_t slot, const Buffer &buffer, std::uint64_t offset = 0) {
    nybler_render_pass_set_vertex_buffer(handle_, slot, buffer.native_handle(), offset);
  }

  void draw(
    std::uint32_t vertex_count,
    std::uint32_t instance_count = 1,
    std::uint32_t first_vertex = 0,
    std::uint32_t first_instance = 0
  ) {
    nybler_render_pass_draw(
      handle_, vertex_count, instance_count, first_vertex, first_instance);
  }
};

class CommandEncoder
  : public detail::OwnedHandle<NyblerCommandEncoder, nybler_command_encoder_release> {
 public:
  using OwnedHandle::OwnedHandle;

  NyblerResult begin_render_pass(
    const NyblerRenderPassDescriptor &descriptor,
    RenderPassEncoder &out_pass
  ) {
    NyblerRenderPassEncoder *raw = nullptr;
    const NyblerResult result =
      nybler_command_encoder_begin_render_pass(handle_, &descriptor, &raw);
    if (result == NYBLER_OK) out_pass.reset(raw);
    return result;
  }

  NyblerResult copy_texture_to_buffer(
    const NyblerImageCopyTexture &source,
    const NyblerImageCopyBuffer &destination,
    const NyblerExtent3D &copy_size
  ) {
    return nybler_command_encoder_copy_texture_to_buffer(
      handle_, &source, &destination, &copy_size);
  }

  NyblerResult copy_texture_to_buffer(
    const Texture &texture,
    const Buffer &buffer,
    const NyblerExtent3D &copy_size,
    std::uint32_t bytes_per_row,
    std::uint64_t destination_offset = 0,
    const NyblerOrigin3D &origin = NyblerOrigin3D(),
    std::uint32_t mip_level = 0,
    std::uint32_t rows_per_image = 0
  ) {
    NyblerImageCopyTexture source{};
    source.texture = texture.native_handle();
    source.mip_level = mip_level;
    source.origin = origin;

    NyblerImageCopyBuffer destination{};
    destination.buffer = buffer.native_handle();
    destination.offset = destination_offset;
    destination.bytes_per_row = bytes_per_row;
    destination.rows_per_image = rows_per_image;

    return copy_texture_to_buffer(source, destination, copy_size);
  }

  NyblerResult finish(CommandBuffer &out_commands) {
    NyblerCommandBuffer *raw = nullptr;
    const NyblerResult result = nybler_command_encoder_finish(handle_, &raw);
    if (result == NYBLER_OK) out_commands.reset(raw);
    return result;
  }
};

class Queue : public detail::BorrowedHandle<NyblerQueue> {
 public:
  using BorrowedHandle::BorrowedHandle;

  NyblerResult submit(CommandBuffer &commands) {
    NyblerCommandBuffer *raw = commands.native_handle();
    const NyblerResult result = nybler_queue_submit(handle_, 1, &raw);
    /* nybler_queue_submit() consumes command buffers on every accepted call.
     * Detach unconditionally so a backend failure cannot cause a double free. */
    commands.release();
    return result;
  }

  NyblerResult submit(std::initializer_list<CommandBuffer *> command_buffers) {
    std::vector<NyblerCommandBuffer *> raw;
    raw.reserve(command_buffers.size());
    for (CommandBuffer *commands : command_buffers) {
      raw.push_back(commands ? commands->native_handle() : nullptr);
    }

    const NyblerResult result =
      nybler_queue_submit(handle_, raw.size(), raw.data());
    for (CommandBuffer *commands : command_buffers) {
      if (commands) commands->release();
    }
    return result;
  }
};

class TextRenderer
  : public detail::OwnedHandle<NyblerTextRenderer, nybler_text_renderer_release> {
 public:
  using OwnedHandle::OwnedHandle;
};

class Painter : public detail::OwnedHandle<NyblerPainter, nybler_painter_release> {
 public:
  using OwnedHandle::OwnedHandle;

  NyblerResult record(
    CommandEncoder &encoder,
    const NyblerViewport &viewport,
    const NyblerDisplayList *display_list,
    const NyblerRenderPassDescriptor &render_pass
  ) {
    return nybler_painter_record(
      handle_, encoder.native_handle(), &viewport, display_list, &render_pass);
  }

  void set_text_renderer(TextRenderer &renderer) {
    nybler_painter_set_text_renderer(handle_, renderer.native_handle());
  }

  void clear_text_renderer() {
    nybler_painter_set_text_renderer(handle_, nullptr);
  }
};

inline NyblerResult create_buffer(
  NyblerDevice *device,
  const NyblerBufferDescriptor &descriptor,
  Buffer *out_buffer
) {
  if (!out_buffer) return NYBLER_ERROR_INVALID_ARGUMENT;
  NyblerBuffer *raw = nullptr;
  const NyblerResult result = nybler_device_create_buffer(device, &descriptor, &raw);
  if (result == NYBLER_OK) *out_buffer = Buffer(raw);
  return result;
}

inline NyblerResult create_texture(
  NyblerDevice *device,
  const NyblerTextureDescriptor &descriptor,
  Texture *out_texture
) {
  if (!out_texture) return NYBLER_ERROR_INVALID_ARGUMENT;
  NyblerTexture *raw = nullptr;
  const NyblerResult result = nybler_device_create_texture(device, &descriptor, &raw);
  if (result == NYBLER_OK) *out_texture = Texture(raw);
  return result;
}

inline NyblerResult create_command_encoder(
  NyblerDevice *device,
  CommandEncoder *out_encoder
) {
  if (!out_encoder) return NYBLER_ERROR_INVALID_ARGUMENT;
  NyblerCommandEncoder *raw = nullptr;
  const NyblerResult result = nybler_device_create_command_encoder(device, &raw);
  if (result == NYBLER_OK) *out_encoder = CommandEncoder(raw);
  return result;
}

inline Queue device_queue(NyblerDevice *device) {
  return Queue(nybler_device_get_queue(device));
}

inline NyblerResult create_text_renderer(
  NyblerDevice *device,
  const char *font_path,
  float font_size,
  TextRenderer *out_renderer
) {
  if (!out_renderer) return NYBLER_ERROR_INVALID_ARGUMENT;
  NyblerTextRenderer *raw = nullptr;
  const NyblerResult result =
    nybler_text_renderer_create(device, font_path, font_size, &raw);
  if (result == NYBLER_OK) *out_renderer = TextRenderer(raw);
  return result;
}

inline NyblerResult create_painter(NyblerDevice *device, Painter *out_painter) {
  if (!out_painter) return NYBLER_ERROR_INVALID_ARGUMENT;
  NyblerPainter *raw = nullptr;
  const NyblerResult result = nybler_painter_create(device, &raw);
  if (result == NYBLER_OK) *out_painter = Painter(raw);
  return result;
}

}  // namespace nybler

#endif /* NYBLER_NYBLER_HPP */
