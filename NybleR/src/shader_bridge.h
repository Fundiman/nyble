#ifndef NYBLER_SHADER_BRIDGE_H_
#define NYBLER_SHADER_BRIDGE_H_

#include "nybler/nybler.h"

#ifdef __cplusplus
extern "C" {
#endif

NyblerResult nybler_tint_compile(
  NyblerShaderFormat source_format,
  const void *source,
  size_t source_size,
  NyblerShaderFormat target_format,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
);

/* WGSL binding point -> flat GLSL binding number, plus the WebGPU binding
 * type so Tint can name combined texture/sampler uniforms deterministically. */
typedef struct NyblerGlslBinding {
  uint32_t group;
  uint32_t binding;
  uint32_t glsl_binding;
  NyblerBindingType type;
} NyblerGlslBinding;

typedef enum NyblerGlslStage {
  NYBLER_GLSL_STAGE_UNKNOWN = 0,
  NYBLER_GLSL_STAGE_VERTEX,
  NYBLER_GLSL_STAGE_FRAGMENT
} NyblerGlslStage;

/* Generate GLSL ES 300 for one pipeline entry point. Unlike the standalone
 * compiler, this uses the pipeline's entry point name and binds every WGSL
 * resource at the flat binding number assigned by the pipeline layout. */
NyblerResult nybler_tint_translate_glsl(
  const void *source,
  size_t source_size,
  const char *entry_point,
  NyblerGlslStage stage,
  const NyblerGlslBinding *bindings,
  size_t binding_count,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
);

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_SHADER_BRIDGE_H_ */
