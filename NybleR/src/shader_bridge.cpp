#include "shader_bridge.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "src/tint/api/tint.h"
#include "src/tint/lang/glsl/writer/writer.h"
#include "src/tint/lang/spirv/writer/writer.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/utils/diagnostic/diagnostic.h"

namespace {

struct TintRuntime {
  TintRuntime() { tint::Initialize(); }
  ~TintRuntime() { tint::Shutdown(); }
};

TintRuntime& Runtime() {
  static TintRuntime runtime;
  return runtime;
}

void SetError(char **out_error, const std::string &message) {
  if (!out_error || *out_error) return;
  size_t length = message.size();
  char *copy = static_cast<char *>(malloc(length + 1));
  if (!copy) return;
  memcpy(copy, message.c_str(), length + 1);
  *out_error = copy;
}

uint8_t *CopyBytes(const void *data, size_t size) {
  uint8_t *copy = static_cast<uint8_t *>(malloc(size ? size : 1));
  if (!copy) return nullptr;
  if (size) memcpy(copy, data, size);
  return copy;
}

NyblerResult CompileWgsl(
  const void *source,
  size_t source_size,
  NyblerShaderFormat target,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
) {
  Runtime();

  std::string text(
    static_cast<const char *>(source),
    static_cast<const char *>(source) + source_size
  );
  tint::Source::File file("nybler.wgsl", text);

  tint::wgsl::reader::Options options;
  options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
  auto ir = tint::wgsl::reader::WgslToIR(&file, options);
  if (ir != tint::Success) {
    SetError(out_error, ir.Failure().reason.Str());
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  if (target == NYBLER_SHADER_FORMAT_SPIRV) {
    tint::spirv::writer::Options spirv_options;
    auto output = tint::spirv::writer::Generate(ir.Get(), spirv_options);
    if (output != tint::Success) {
      SetError(out_error, output.Failure().reason.Str());
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
    size_t size = output->spirv.size() * sizeof(uint32_t);
    uint8_t *code = CopyBytes(output->spirv.data(), size);
    if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
    *out_code = code;
    *out_code_size = size;
    return NYBLER_OK;
  }

  if (target == NYBLER_SHADER_FORMAT_GLSL) {
    tint::glsl::writer::Options glsl_options;
    glsl_options.version = tint::glsl::writer::Version(
      tint::glsl::writer::Version::Standard::kES,
      3,
      10
    );
    auto output = tint::glsl::writer::Generate(ir.Get(), glsl_options, "main");
    if (output != tint::Success) {
      SetError(out_error, output.Failure().reason.Str());
      return NYBLER_ERROR_INVALID_ARGUMENT;
    }
    uint8_t *code = CopyBytes(output->glsl.data(), output->glsl.size());
    if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
    *out_code = code;
    *out_code_size = output->glsl.size();
    return NYBLER_OK;
  }

  if (target == NYBLER_SHADER_FORMAT_WGSL) {
    uint8_t *code = CopyBytes(text.data(), text.size());
    if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
    *out_code = code;
    *out_code_size = text.size();
    return NYBLER_OK;
  }

  SetError(out_error, "unsupported WGSL output format");
  return NYBLER_ERROR_UNSUPPORTED;
}

NyblerResult CopySpirv(
  const void *source,
  size_t source_size,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
) {
  static const unsigned char kSpirvMagic[4] = {0x03, 0x02, 0x23, 0x07};

  if (source_size < 16 || source_size % 4 != 0 ||
      memcmp(source, kSpirvMagic, sizeof(kSpirvMagic)) != 0) {
    SetError(out_error, "input is not a SPIR-V module");
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  uint8_t *code = CopyBytes(source, source_size);
  if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
  *out_code = code;
  *out_code_size = source_size;
  return NYBLER_OK;
}

}  // namespace

extern "C" NyblerResult nybler_tint_compile(
  NyblerShaderFormat source_format,
  const void *source,
  size_t source_size,
  NyblerShaderFormat target_format,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
) {
  if (!out_code || !out_code_size) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_code = nullptr;
  *out_code_size = 0;
  if (out_error) *out_error = nullptr;
  if (!source || source_size == 0) {
    SetError(out_error, "empty shader source");
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  try {
    if (source_format == NYBLER_SHADER_FORMAT_WGSL) {
      return CompileWgsl(source, source_size, target_format, out_code, out_code_size, out_error);
    }
    if (source_format == NYBLER_SHADER_FORMAT_SPIRV &&
        target_format == NYBLER_SHADER_FORMAT_SPIRV) {
      return CopySpirv(source, source_size, out_code, out_code_size, out_error);
    }
    if (source_format == target_format &&
        target_format != NYBLER_SHADER_FORMAT_UNKNOWN) {
      uint8_t *code = CopyBytes(source, source_size);
      if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
      *out_code = code;
      *out_code_size = source_size;
      return NYBLER_OK;
    }
    SetError(out_error, "unsupported shader conversion");
    return NYBLER_ERROR_UNSUPPORTED;
  } catch (const std::bad_alloc &) {
    SetError(out_error, "out of memory while compiling shader");
    return NYBLER_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    SetError(out_error, "Tint shader compilation failed");
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
}
