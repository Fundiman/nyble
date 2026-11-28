#include "nybler/nybler.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
  if (condition) return 1;
  fprintf(stderr, "shader smoke failed: %s\n", message);
  return 0;
}

int main(void) {
  static const char wgsl[] =
    "@compute @workgroup_size(1)\n"
    "fn main() {\n"
    "}\n";
  static const char invalid_wgsl[] = "fn main() {";
  static const unsigned char spirv_magic[4] = {0x03, 0x02, 0x23, 0x07};
  static const char glsl_version[] = "#version 300 es";

  NyblerInitOptions options = {1, NYBLER_BACKEND_AUTO, 1, NULL};
  NyblerInstance *instance = NULL;
  NyblerAdapter *adapter = NULL;
  NyblerDevice *device = NULL;
  NyblerShaderModule *module = NULL;
  uint8_t *spirv = NULL;
  uint8_t *spirv_copy = NULL;
  uint8_t *glsl = NULL;
  size_t spirv_size = 0;
  size_t spirv_copy_size = 0;
  size_t glsl_size = 0;
  char *error = NULL;
  int ok = 1;

  ok = check(
    nybler_shader_compile(
      NYBLER_SHADER_FORMAT_WGSL,
      wgsl,
      sizeof(wgsl) - 1,
      NYBLER_SHADER_FORMAT_SPIRV,
      &spirv,
      &spirv_size,
      &error
    ) == NYBLER_OK,
    "compile WGSL to SPIR-V"
  ) && ok;
  if (error) {
    fprintf(stderr, "unexpected diagnostic: %s\n", error);
    ok = 0;
    nybler_string_release(error);
    error = NULL;
  }
  if (!ok) goto done;

  ok = check(spirv_size >= 16, "SPIR-V size") && ok;
  ok = check(
    spirv && memcmp(spirv, spirv_magic, sizeof(spirv_magic)) == 0,
    "SPIR-V magic"
  ) && ok;

  ok = check(
    nybler_shader_compile(
      NYBLER_SHADER_FORMAT_SPIRV,
      spirv,
      spirv_size,
      NYBLER_SHADER_FORMAT_SPIRV,
      &spirv_copy,
      &spirv_copy_size,
      &error
    ) == NYBLER_OK,
    "copy SPIR-V"
  ) && ok;
  ok = check(spirv_copy_size == spirv_size, "SPIR-V copy size") && ok;
  ok = check(
    spirv_copy && memcmp(spirv_copy, spirv, spirv_size) == 0,
    "SPIR-V copy contents"
  ) && ok;

  ok = check(
    nybler_shader_compile(
      NYBLER_SHADER_FORMAT_WGSL,
      wgsl,
      sizeof(wgsl) - 1,
      NYBLER_SHADER_FORMAT_GLSL,
      &glsl,
      &glsl_size,
      &error
    ) == NYBLER_OK,
    "compile WGSL to GLSL"
  ) && ok;
  ok = check(
    glsl && glsl_size >= sizeof(glsl_version) - 1 &&
    memcmp(glsl, glsl_version, sizeof(glsl_version) - 1) == 0,
    "GLSL ES 300 version header"
  ) && ok;

  ok = check(
    nybler_shader_compile(
      NYBLER_SHADER_FORMAT_WGSL,
      invalid_wgsl,
      sizeof(invalid_wgsl) - 1,
      NYBLER_SHADER_FORMAT_SPIRV,
      &spirv_copy,
      &spirv_copy_size,
      &error
    ) != NYBLER_OK,
    "reject invalid WGSL"
  ) && ok;
  ok = check(error && *error != '\0', "invalid WGSL diagnostic") && ok;
  nybler_string_release(error);
  error = NULL;

  ok = check(nybler_init(&options, &instance) == NYBLER_OK, "init") && ok;
  ok = check(
    nybler_instance_request_adapter(instance, &adapter) == NYBLER_OK,
    "request adapter"
  ) && ok;
  ok = check(
    nybler_adapter_request_device(adapter, &device) == NYBLER_OK,
    "request device"
  ) && ok;
  ok = check(
    nybler_shader_module_create(
      device,
      NYBLER_SHADER_FORMAT_WGSL,
      wgsl,
      sizeof(wgsl) - 1,
      "smoke",
      &module
    ) == NYBLER_OK,
    "create shader module"
  ) && ok;

  printf(
    "nybler shader smoke: WGSL -> SPIR-V %zu bytes, GLSL %zu bytes\n",
    spirv_size,
    glsl_size
  );

done:
  nybler_string_release(error);
  nybler_shader_module_release(module);
  nybler_device_release(device);
  nybler_adapter_release(adapter);
  nybler_shutdown(instance);
  nybler_shader_code_release(glsl);
  nybler_shader_code_release(spirv_copy);
  nybler_shader_code_release(spirv);
  return ok ? 0 : 1;
}
