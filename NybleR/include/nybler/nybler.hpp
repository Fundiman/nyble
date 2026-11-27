#ifndef NYBLER_NYBLER_HPP
#define NYBLER_NYBLER_HPP

#include "nybler/nybler.h"

#include <cstdint>
#include <string>

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

}  // namespace nybler

#endif /* NYBLER_NYBLER_HPP */
