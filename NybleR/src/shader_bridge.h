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

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_SHADER_BRIDGE_H_ */
