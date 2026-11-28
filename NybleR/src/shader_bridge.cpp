#include "shader_bridge.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "src/tint/api/tint.h"
#include "src/tint/lang/glsl/writer/writer.h"
#include "src/tint/lang/spirv/writer/writer.h"
#include "src/tint/lang/wgsl/ast/transform/manager.h"
#include "src/tint/lang/wgsl/ast/transform/renamer.h"
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

size_t FirstNonSpace(const std::string &text, size_t offset) {
  while (offset < text.size() &&
         std::isspace(static_cast<unsigned char>(text[offset]))) {
    ++offset;
  }
  return offset;
}

bool IsIdentifierChar(char value) {
  unsigned char byte = static_cast<unsigned char>(value);
  return std::isalnum(byte) != 0 || value == '_';
}

bool TokenAt(const std::string &text, size_t offset, const char *token) {
  size_t length = std::strlen(token);

  if (text.compare(offset, length, token) != 0) return false;
  if (offset + length == text.size()) return true;
  char next = text[offset + length];
  return !std::isalnum(static_cast<unsigned char>(next)) && next != '_';
}

/* Skip interpolation/storage qualifiers between `layout(...)` and `in`/`out`. */
size_t AfterLayoutQualifiers(const std::string &line, size_t offset) {
  static const char *const qualifiers[] = {
    "flat", "smooth", "noperspective", "centroid", "sample", "invariant"
  };

  for (;;) {
    size_t i;
    size_t qualifier_length = 0;

    for (i = 0; i < sizeof(qualifiers) / sizeof(qualifiers[0]); ++i) {
      if (TokenAt(line, offset, qualifiers[i])) {
        qualifier_length = std::strlen(qualifiers[i]);
        break;
      }
    }
    if (!qualifier_length) return offset;
    offset += qualifier_length;
    offset = FirstNonSpace(line, offset);
  }
}

bool InterstageDeclaration(
  const std::string &line,
  uint32_t *location,
  std::string *name,
  bool *is_input,
  size_t *direction_offset
) {
  size_t indent_end = FirstNonSpace(line, 0);
  size_t equal;
  size_t close;
  size_t end;
  size_t start;
  char *parse_end = NULL;
  unsigned long parsed;

  if (indent_end == line.size() || !TokenAt(line, indent_end, "layout")) {
    return false;
  }
  equal = line.find('=', indent_end);
  close = line.find(')', indent_end);
  if (equal == std::string::npos || close == std::string::npos ||
      equal > close) {
    return false;
  }
  parsed = std::strtoul(line.c_str() + equal + 1, &parse_end, 10);
  if (parse_end == line.c_str() + equal + 1) return false;

  *direction_offset = AfterLayoutQualifiers(line, FirstNonSpace(line, close + 1));
  if (TokenAt(line, *direction_offset, "in")) {
    *is_input = true;
  } else if (TokenAt(line, *direction_offset, "out")) {
    *is_input = false;
  } else {
    return false;
  }

  end = line.find(';', *direction_offset);
  if (end == std::string::npos) return false;
  while (end > *direction_offset &&
         std::isspace(static_cast<unsigned char>(line[end - 1]))) {
    --end;
  }
  start = end;
  while (start > *direction_offset && IsIdentifierChar(line[start - 1])) {
    --start;
  }
  if (start == end) return false;
  *location = (uint32_t)parsed;
  *name = line.substr(start, end - start);
  return true;
}

void ReplaceIdentifier(
  std::string *text,
  const std::string &from,
  const std::string &to
) {
  size_t offset = 0;

  while ((offset = text->find(from, offset)) != std::string::npos) {
    bool before = offset == 0 || !IsIdentifierChar((*text)[offset - 1]);
    size_t end = offset + from.size();
    bool after = end == text->size() || !IsIdentifierChar((*text)[end]);

    if (before && after) {
      text->replace(offset, from.size(), to);
      offset += to.size();
    } else {
      offset = end;
    }
  }
}

/* GLSL ES 300 allows explicit locations on vertex inputs and fragment outputs,
 * but rejects them on inter-stage variables. Tint also gives the two stages
 * different generated names for the same varying, so canonicalize the names by
 * location before dropping the qualifier. */
std::string NormalizeGlsl(const std::string &source, NyblerGlslStage stage) {
  bool strip_input = stage == NYBLER_GLSL_STAGE_FRAGMENT;
  bool strip_output = stage == NYBLER_GLSL_STAGE_VERTEX;
  std::vector<std::pair<std::string, std::string> > renames;
  std::string result;
  size_t offset = 0;

  if (!strip_input && !strip_output) return source;
  while (offset <= source.size()) {
    size_t end = source.find('\n', offset);
    std::string line = end == std::string::npos
      ? source.substr(offset)
      : source.substr(offset, end - offset);
    uint32_t location = 0;
    std::string name;
    bool is_input = false;
    size_t direction_offset = 0;

    if (InterstageDeclaration(
          line, &location, &name, &is_input, &direction_offset) &&
        ((strip_input && is_input) || (strip_output && !is_input))) {
      std::string canonical = "nybler_varying_" + std::to_string(location);
      if (name != canonical) renames.push_back(std::make_pair(name, canonical));
    }
    if (end == std::string::npos) break;
    offset = end + 1;
  }

  std::string named = source;
  for (size_t i = 0; i < renames.size(); ++i) {
    ReplaceIdentifier(&named, renames[i].first, renames[i].second);
  }

  offset = 0;
  while (offset <= named.size()) {
    size_t end = named.find('\n', offset);
    std::string line = end == std::string::npos
      ? named.substr(offset)
      : named.substr(offset, end - offset);
    uint32_t location = 0;
    std::string name;
    bool is_input = false;
    size_t direction_offset = 0;

    if (InterstageDeclaration(
          line, &location, &name, &is_input, &direction_offset) &&
        ((strip_input && is_input) || (strip_output && !is_input))) {
      size_t indent_end = FirstNonSpace(line, 0);
      line = line.substr(0, indent_end) + line.substr(direction_offset);
    }
    result += line;
    if (end == std::string::npos) break;
    result += '\n';
    offset = end + 1;
  }
  return result;
}

NyblerResult TranslateGlsl(
  const void *source,
  size_t source_size,
  const char *entry_point,
  NyblerGlslStage stage,
  const NyblerGlslBinding *bindings,
  size_t binding_count,
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
  tint::wgsl::reader::Options parse_options;
  parse_options.allowed_features = tint::wgsl::AllowedFeatures::Everything();
  auto program = tint::wgsl::reader::Parse(&file, parse_options);
  if (!program.IsValid()) {
    SetError(out_error, program.Diagnostics().Str());
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }

  /* Tint's GLSL writer does not rename WGSL identifiers that collide with GLSL
   * keywords. Run its dedicated renamer first, as the GLSL writer test helper
   * does, so arbitrary valid WGSL can be translated. */
  tint::ast::transform::Manager rename_manager;
  tint::ast::transform::DataMap rename_inputs;
  tint::ast::transform::DataMap rename_outputs;
  rename_inputs.Add<tint::ast::transform::Renamer::Config>(
    tint::ast::transform::Renamer::Target::kGlslKeywords,
    /* preserve_unicode */ false);
  rename_manager.Add<tint::ast::transform::Renamer>();
  auto renamed = rename_manager.Run(program, rename_inputs, rename_outputs);
  if (!renamed.IsValid()) {
    SetError(out_error, renamed.Diagnostics().Str());
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  std::string target_entry = entry_point ? entry_point : "main";
  if (auto *remapped = rename_outputs.Get<tint::ast::transform::Renamer::Data>()) {
    auto found = remapped->remappings.find(target_entry);
    if (found != remapped->remappings.end()) target_entry = found->second;
  }

  tint::glsl::writer::Options glsl_options;
  /* Mesa's local GLES context is version 3.0, so emit "#version 300 es".
   * Tint encodes the version as major followed by minor followed by "0". */
  glsl_options.version = tint::glsl::writer::Version(
    tint::glsl::writer::Version::Standard::kES, 3, 0);
  if (bindings) {
    for (size_t i = 0; i < binding_count; ++i) {
      glsl_options.binding_remapper_options.binding_points.emplace(
        tint::BindingPoint{bindings[i].group, bindings[i].binding},
        tint::BindingPoint{0, bindings[i].glsl_binding});
    }
  }

  /* The IR GLSL printer in this Tint snapshot is incomplete; the AST path is
   * the supported generator and is what the Tint CLI uses. */
  auto output = tint::glsl::writer::Generate(
    renamed, glsl_options, target_entry);
  if (output != tint::Success) {
    SetError(out_error, output.Failure().reason.Str());
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  std::string glsl = NormalizeGlsl(output->glsl, stage);
  /* Mesa consumes GLSL as a C string; keep the reported size exact while
   * storing a terminating byte that is not part of the shader source. */
  uint8_t *code = static_cast<uint8_t *>(malloc(glsl.size() + 1));
  if (!code) return NYBLER_ERROR_OUT_OF_MEMORY;
  if (!glsl.empty()) memcpy(code, glsl.data(), glsl.size());
  code[glsl.size()] = '\0';
  *out_code = code;
  *out_code_size = glsl.size();
  return NYBLER_OK;
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
    return TranslateGlsl(
      source, source_size, "main", NYBLER_GLSL_STAGE_UNKNOWN, NULL, 0,
      out_code, out_code_size, out_error);
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

extern "C" NyblerResult nybler_tint_translate_glsl(
  const void *source,
  size_t source_size,
  const char *entry_point,
  NyblerGlslStage stage,
  const NyblerGlslBinding *bindings,
  size_t binding_count,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
) {
  if (!out_code || !out_code_size || !source || source_size == 0) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  *out_code = nullptr;
  *out_code_size = 0;
  if (out_error) *out_error = nullptr;
  try {
    return TranslateGlsl(
      source, source_size, entry_point, stage, bindings, binding_count,
      out_code, out_code_size, out_error);
  } catch (const std::bad_alloc &) {
    SetError(out_error, "out of memory while generating GLSL");
    return NYBLER_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    SetError(out_error, "Tint GLSL generation failed");
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
}

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
