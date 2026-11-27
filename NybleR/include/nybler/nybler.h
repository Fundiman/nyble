#ifndef NYBLER_NYBLER_H
#define NYBLER_NYBLER_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(NYBLER_BUILD_SHARED)
#define NYBLER_API __declspec(dllexport)
#elif defined(_WIN32) && defined(NYBLER_USE_SHARED)
#define NYBLER_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define NYBLER_API __attribute__((visibility("default")))
#else
#define NYBLER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NyblerResult {
  NYBLER_OK = 0,
  NYBLER_ERROR_INVALID_ARGUMENT = -1,
  NYBLER_ERROR_UNSUPPORTED = -2,
  NYBLER_ERROR_OUT_OF_MEMORY = -3,
  NYBLER_ERROR_BACKEND_UNAVAILABLE = -4
} NyblerResult;

typedef enum NyblerBackend {
  NYBLER_BACKEND_AUTO = 0,
  NYBLER_BACKEND_MESA_VULKAN,
  NYBLER_BACKEND_MESA_GL,
  NYBLER_BACKEND_NULL
} NyblerBackend;

typedef enum NyblerShaderFormat {
  NYBLER_SHADER_FORMAT_UNKNOWN = 0,
  NYBLER_SHADER_FORMAT_WGSL,
  NYBLER_SHADER_FORMAT_SPIRV,
  NYBLER_SHADER_FORMAT_GLSL,
  NYBLER_SHADER_FORMAT_MSL,
  NYBLER_SHADER_FORMAT_HLSL
} NyblerShaderFormat;

typedef struct NyblerInstance NyblerInstance;
typedef struct NyblerAdapter NyblerAdapter;
typedef struct NyblerDevice NyblerDevice;
typedef struct NyblerShaderModule NyblerShaderModule;
typedef struct NyblerBuffer NyblerBuffer;
typedef struct NyblerTexture NyblerTexture;
typedef struct NyblerSampler NyblerSampler;
typedef struct NyblerBindGroupLayout NyblerBindGroupLayout;
typedef struct NyblerBindGroup NyblerBindGroup;
typedef struct NyblerRenderPipeline NyblerRenderPipeline;
typedef struct NyblerCommandEncoder NyblerCommandEncoder;

typedef struct NyblerInitOptions {
  uint32_t version;
  NyblerBackend backend;
  int headless;
  const char *preferred_adapter;
} NyblerInitOptions;

NYBLER_API const char *nybler_version_string(void);
NYBLER_API uint32_t nybler_version_number(void);

NYBLER_API NyblerResult nybler_init(
  const NyblerInitOptions *options,
  NyblerInstance **out_instance
);
NYBLER_API void nybler_shutdown(NyblerInstance *instance);

NYBLER_API NyblerResult nybler_shader_module_create(
  NyblerDevice *device,
  NyblerShaderFormat format,
  const void *code,
  size_t code_size,
  const char *label,
  NyblerShaderModule **out_module
);
NYBLER_API void nybler_shader_module_release(NyblerShaderModule *module);

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_NYBLER_H */
