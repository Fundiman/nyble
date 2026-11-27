#include "nybler/nybler.h"

#include <stdlib.h>

struct NyblerInstance {
  NyblerBackend backend;
  int headless;
};

struct NyblerDevice {
  NyblerInstance *instance;
};

NyblerResult nybler_init(
  const NyblerInitOptions *options,
  NyblerInstance **out_instance
) {
  if (!options || !out_instance) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_instance = NULL;
  if (options->version != 1) return NYBLER_ERROR_UNSUPPORTED;

  /* Backends are wired in after the local dependency build exists. */
  return NYBLER_ERROR_BACKEND_UNAVAILABLE;
}

void nybler_shutdown(NyblerInstance *instance) {
  free(instance);
}

NyblerResult nybler_shader_module_create(
  NyblerDevice *device,
  NyblerShaderFormat format,
  const void *code,
  size_t code_size,
  const char *label,
  NyblerShaderModule **out_module
) {
  (void)device;
  (void)format;
  (void)code;
  (void)code_size;
  (void)label;
  if (!out_module) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_module = NULL;
  return NYBLER_ERROR_UNSUPPORTED;
}

void nybler_shader_module_release(NyblerShaderModule *module) {
  (void)module;
}
