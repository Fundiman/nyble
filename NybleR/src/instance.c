#include "internal.h"
#include "mesa_backend.h"

#include "shader_bridge.h"

#include <stdlib.h>
#include <string.h>

NyblerResult nybler_init(
  const NyblerInitOptions *options,
  NyblerInstance **out_instance
) {
  NyblerInstance *instance;

  if (!options || !out_instance) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_instance = NULL;
  if (options->version != 1) return NYBLER_ERROR_UNSUPPORTED;

  if (options->backend != NYBLER_BACKEND_AUTO &&
      options->backend != NYBLER_BACKEND_MESA_GL &&
      options->backend != NYBLER_BACKEND_NULL) {
    /* Mesa Vulkan needs a local LLVM-capable driver build. */
    return NYBLER_ERROR_BACKEND_UNAVAILABLE;
  }

  instance = (NyblerInstance *)calloc(1, sizeof(*instance));
  if (!instance) return NYBLER_ERROR_OUT_OF_MEMORY;
  instance->backend = options->backend;
  instance->headless = options->headless ? 1 : 0;

  if (options->preferred_adapter) {
    instance->preferred_adapter =
      nybler_internal_duplicate_string(options->preferred_adapter);
    if (!instance->preferred_adapter) {
      free(instance);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }

  *out_instance = instance;
  return NYBLER_OK;
}

void nybler_shutdown(NyblerInstance *instance) {
  if (!instance) return;
  free(instance->preferred_adapter);
  free(instance);
}

NyblerResult nybler_instance_request_adapter(
  NyblerInstance *instance,
  NyblerAdapter **out_adapter
) {
  NyblerAdapter *adapter;

  if (!instance || !out_adapter) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_adapter = NULL;

  adapter = (NyblerAdapter *)calloc(1, sizeof(*adapter));
  if (!adapter) return NYBLER_ERROR_OUT_OF_MEMORY;
  adapter->instance = instance;
  *out_adapter = adapter;
  return NYBLER_OK;
}

void nybler_adapter_release(NyblerAdapter *adapter) {
  free(adapter);
}

NyblerResult nybler_adapter_request_device(
  NyblerAdapter *adapter,
  NyblerDevice **out_device
) {
  NyblerDevice *device;
  NyblerMesaContext *context = NULL;
  NyblerResult result;

  if (!adapter || !adapter->instance || !out_device) {
    return NYBLER_ERROR_INVALID_ARGUMENT;
  }
  *out_device = NULL;

  device = (NyblerDevice *)calloc(1, sizeof(*device));
  if (!device) return NYBLER_ERROR_OUT_OF_MEMORY;
  device->adapter = adapter;
  device->backend = adapter->instance->backend;

  device->queue = (NyblerQueue *)calloc(1, sizeof(*device->queue));
  if (!device->queue) {
    free(device);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  device->queue->device = device;

  if (device->backend == NYBLER_BACKEND_AUTO ||
      device->backend == NYBLER_BACKEND_MESA_GL) {
    result = nybler_mesa_init(&context);
    if (result == NYBLER_OK) {
      device->backend = NYBLER_BACKEND_MESA_GL;
      device->backend_context = context;
    } else if (adapter->instance->backend == NYBLER_BACKEND_MESA_GL) {
      free(device->queue);
      free(device);
      return result;
    } else {
      device->backend = NYBLER_BACKEND_NULL;
    }
  } else {
    device->backend = NYBLER_BACKEND_NULL;
  }

  *out_device = device;
  return NYBLER_OK;
}

void nybler_device_release(NyblerDevice *device) {
  if (!device) return;
  nybler_mesa_destroy((NyblerMesaContext *)device->backend_context);
  free(device->queue);
  free(device);
}

NyblerResult nybler_shader_compile(
  NyblerShaderFormat source_format,
  const void *source,
  size_t source_size,
  NyblerShaderFormat target_format,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
) {
  return nybler_tint_compile(
    source_format,
    source,
    source_size,
    target_format,
    out_code,
    out_code_size,
    out_error
  );
}

void nybler_shader_code_release(uint8_t *code) {
  free(code);
}

NyblerResult nybler_shader_module_create(
  NyblerDevice *device,
  NyblerShaderFormat format,
  const void *code,
  size_t code_size,
  const char *label,
  NyblerShaderModule **out_module
) {
  NyblerShaderModule *module;
  uint8_t *compiled = NULL;
  size_t compiled_size = 0;
  NyblerResult result;
  char *label_copy = NULL;

  if (!device || !out_module) return NYBLER_ERROR_INVALID_ARGUMENT;
  *out_module = NULL;

  result = nybler_shader_compile(
    format,
    code,
    code_size,
    NYBLER_SHADER_FORMAT_SPIRV,
    &compiled,
    &compiled_size,
    NULL
  );
  if (result != NYBLER_OK) return result;

  if (label) {
    label_copy = nybler_internal_duplicate_string(label);
    if (!label_copy) {
      nybler_shader_code_release(compiled);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
  }

  module = (NyblerShaderModule *)calloc(1, sizeof(*module));
  if (!module) {
    nybler_shader_code_release(compiled);
    free(label_copy);
    return NYBLER_ERROR_OUT_OF_MEMORY;
  }
  module->device = device;
  module->format = NYBLER_SHADER_FORMAT_SPIRV;
  module->code = compiled;
  module->code_size = compiled_size;
  module->label = label_copy;
  if (format == NYBLER_SHADER_FORMAT_WGSL) {
    module->source = (uint8_t *)malloc(code_size ? code_size : 1);
    if (!module->source) {
      nybler_shader_module_release(module);
      return NYBLER_ERROR_OUT_OF_MEMORY;
    }
    if (code_size) memcpy(module->source, code, code_size);
    module->source_size = code_size;
  }
  *out_module = module;
  return NYBLER_OK;
}

void nybler_shader_module_release(NyblerShaderModule *module) {
  if (!module) return;
  nybler_shader_code_release(module->code);
  nybler_shader_code_release(module->source);
  free(module->label);
  free(module);
}
