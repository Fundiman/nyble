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

/* WHATWG DOM Node.nodeType values that NybleR exposes. */
typedef enum NyblerNodeType {
  NYBLER_NODE_TYPE_OTHER = 0,
  NYBLER_NODE_TYPE_ELEMENT = 1,
  NYBLER_NODE_TYPE_ATTRIBUTE = 2,
  NYBLER_NODE_TYPE_TEXT = 3,
  NYBLER_NODE_TYPE_COMMENT = 8,
  NYBLER_NODE_TYPE_DOCUMENT = 9,
  NYBLER_NODE_TYPE_DOCUMENT_TYPE = 10,
  NYBLER_NODE_TYPE_DOCUMENT_FRAGMENT = 11
} NyblerNodeType;

typedef struct NyblerInstance NyblerInstance;
typedef struct NyblerDocument NyblerDocument;
typedef struct NyblerNode NyblerNode;
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
typedef struct NyblerLayout NyblerLayout;
typedef struct NyblerDisplayList NyblerDisplayList;

typedef struct NyblerRect {
  float x;
  float y;
  float width;
  float height;
} NyblerRect;

typedef struct NyblerColor {
  float r;
  float g;
  float b;
  float a;
} NyblerColor;

typedef struct NyblerViewport {
  float width;
  float height;
} NyblerViewport;

typedef enum NyblerDisplayItemKind {
  NYBLER_DISPLAY_ITEM_NONE = 0,
  NYBLER_DISPLAY_ITEM_BOX = 1,
  NYBLER_DISPLAY_ITEM_TEXT = 2
} NyblerDisplayItemKind;

typedef struct NyblerDisplayItem {
  NyblerDisplayItemKind kind;
  NyblerRect rect;
  NyblerColor color;
  NyblerNode *node;
  const char *text;
  size_t text_length;
} NyblerDisplayItem;

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

/* Adapters borrow their instance. Devices borrow their adapter and must be
 * released before the adapter or instance. */
NYBLER_API NyblerResult nybler_instance_request_adapter(
  NyblerInstance *instance,
  NyblerAdapter **out_adapter
);
NYBLER_API void nybler_adapter_release(NyblerAdapter *adapter);
NYBLER_API NyblerResult nybler_adapter_request_device(
  NyblerAdapter *adapter,
  NyblerDevice **out_device
);
NYBLER_API void nybler_device_release(NyblerDevice *device);

/* DOM: parsed documents own their nodes. Node handles are borrowed and stay
 * valid until nybler_document_release(). */
NYBLER_API NyblerResult nybler_document_parse(
  const char *html,
  size_t length,
  NyblerDocument **out_document
);
NYBLER_API void nybler_document_release(NyblerDocument *document);

NYBLER_API NyblerNode *nybler_document_document_element(
  NyblerDocument *document
);
NYBLER_API NyblerNode *nybler_document_body(NyblerDocument *document);
NYBLER_API NyblerResult nybler_document_elements_by_tag_name(
  NyblerDocument *document,
  const char *tag_name,
  size_t tag_length,
  NyblerNode ***out_elements,
  size_t *out_count
);
NYBLER_API void nybler_node_list_release(NyblerNode **elements, size_t count);

NYBLER_API NyblerNodeType nybler_node_get_node_type(NyblerNode *node);
NYBLER_API NyblerNode *nybler_node_get_first_child(NyblerNode *node);
NYBLER_API NyblerNode *nybler_node_get_next_sibling(NyblerNode *node);
NYBLER_API const char *nybler_node_get_local_name(
  NyblerNode *node,
  size_t *out_length
);
NYBLER_API const char *nybler_node_get_attribute(
  NyblerNode *node,
  const char *name,
  size_t name_length,
  size_t *out_length
);
NYBLER_API NyblerResult nybler_node_get_text_content(
  NyblerNode *node,
  char **out_text,
  size_t *out_length
);
NYBLER_API void nybler_string_release(char *value);

/* Style/layout/display-list skeleton. Layout objects borrow the source
 * document; release the layout before the document. */
NYBLER_API NyblerResult nybler_layout_compute(
  NyblerDocument *document,
  const NyblerViewport *viewport,
  NyblerLayout **out_layout
);
NYBLER_API void nybler_layout_release(NyblerLayout *layout);
NYBLER_API int nybler_layout_get_rect(
  const NyblerLayout *layout,
  NyblerNode *node,
  NyblerRect *out_rect
);

NYBLER_API NyblerResult nybler_layout_display_list(
  const NyblerLayout *layout,
  NyblerDisplayList **out_list
);
NYBLER_API size_t nybler_display_list_length(const NyblerDisplayList *list);
NYBLER_API const NyblerDisplayItem *nybler_display_list_item(
  const NyblerDisplayList *list,
  size_t index
);
NYBLER_API void nybler_display_list_release(NyblerDisplayList *list);

NYBLER_API NyblerResult nybler_shader_module_create(
  NyblerDevice *device,
  NyblerShaderFormat format,
  const void *code,
  size_t code_size,
  const char *label,
  NyblerShaderModule **out_module
);
NYBLER_API void nybler_shader_module_release(NyblerShaderModule *module);

/* Standalone full-WGSL compilation through Tint. `source_format` accepts WGSL
 * or SPIR-V; `target_format` accepts WGSL (identity), SPIR-V, or GLSL. Codes
 * returned here are owned by the caller and must be released with
 * nybler_shader_code_release(). When `out_error` is non-NULL, compile failures
 * write a diagnostic string that must be released with nybler_string_release(). */
NYBLER_API NyblerResult nybler_shader_compile(
  NyblerShaderFormat source_format,
  const void *source,
  size_t source_size,
  NyblerShaderFormat target_format,
  uint8_t **out_code,
  size_t *out_code_size,
  char **out_error
);
NYBLER_API void nybler_shader_code_release(uint8_t *code);

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_NYBLER_H */
