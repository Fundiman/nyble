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

/* WebGPU-style resource flags and enums. Flags are bit masks. */
typedef enum NyblerBufferUsage {
  NYBLER_BUFFER_USAGE_MAP_READ = 0x0001,
  NYBLER_BUFFER_USAGE_MAP_WRITE = 0x0002,
  NYBLER_BUFFER_USAGE_COPY_SRC = 0x0004,
  NYBLER_BUFFER_USAGE_COPY_DST = 0x0008,
  NYBLER_BUFFER_USAGE_INDEX = 0x0010,
  NYBLER_BUFFER_USAGE_VERTEX = 0x0020,
  NYBLER_BUFFER_USAGE_UNIFORM = 0x0040,
  NYBLER_BUFFER_USAGE_STORAGE = 0x0080,
  NYBLER_BUFFER_USAGE_INDIRECT = 0x0100,
  NYBLER_BUFFER_USAGE_QUERY_RESOLVE = 0x0200
} NyblerBufferUsage;

typedef enum NyblerTextureUsage {
  NYBLER_TEXTURE_USAGE_COPY_SRC = 0x0001,
  NYBLER_TEXTURE_USAGE_COPY_DST = 0x0002,
  NYBLER_TEXTURE_USAGE_TEXTURE_BINDING = 0x0004,
  NYBLER_TEXTURE_USAGE_STORAGE_BINDING = 0x0008,
  NYBLER_TEXTURE_USAGE_RENDER_ATTACHMENT = 0x0010
} NyblerTextureUsage;

typedef enum NyblerTextureFormat {
  NYBLER_TEXTURE_FORMAT_UNDEFINED = 0,
  NYBLER_TEXTURE_FORMAT_R8_UNORM,
  NYBLER_TEXTURE_FORMAT_RG8_UNORM,
  NYBLER_TEXTURE_FORMAT_RGBA8_UNORM,
  NYBLER_TEXTURE_FORMAT_BGRA8_UNORM,
  NYBLER_TEXTURE_FORMAT_R16_FLOAT,
  NYBLER_TEXTURE_FORMAT_R32_FLOAT,
  NYBLER_TEXTURE_FORMAT_RG32_FLOAT,
  NYBLER_TEXTURE_FORMAT_RGBA32_FLOAT,
  NYBLER_TEXTURE_FORMAT_DEPTH24_PLUS,
  NYBLER_TEXTURE_FORMAT_DEPTH32_FLOAT
} NyblerTextureFormat;

typedef enum NyblerAddressMode {
  NYBLER_ADDRESS_MODE_CLAMP_TO_EDGE = 0,
  NYBLER_ADDRESS_MODE_REPEAT,
  NYBLER_ADDRESS_MODE_MIRROR_REPEAT
} NyblerAddressMode;

typedef enum NyblerFilterMode {
  NYBLER_FILTER_MODE_NEAREST = 0,
  NYBLER_FILTER_MODE_LINEAR
} NyblerFilterMode;

typedef enum NyblerCompareFunction {
  NYBLER_COMPARE_FUNCTION_NONE = 0,
  NYBLER_COMPARE_FUNCTION_NEVER,
  NYBLER_COMPARE_FUNCTION_LESS,
  NYBLER_COMPARE_FUNCTION_EQUAL,
  NYBLER_COMPARE_FUNCTION_LESS_EQUAL,
  NYBLER_COMPARE_FUNCTION_GREATER,
  NYBLER_COMPARE_FUNCTION_NOT_EQUAL,
  NYBLER_COMPARE_FUNCTION_GREATER_EQUAL,
  NYBLER_COMPARE_FUNCTION_ALWAYS
} NyblerCompareFunction;

typedef enum NyblerShaderStage {
  NYBLER_SHADER_STAGE_VERTEX = 0x0001,
  NYBLER_SHADER_STAGE_FRAGMENT = 0x0002,
  NYBLER_SHADER_STAGE_COMPUTE = 0x0004
} NyblerShaderStage;

typedef enum NyblerBindingType {
  NYBLER_BINDING_TYPE_UNIFORM_BUFFER = 0,
  NYBLER_BINDING_TYPE_STORAGE_BUFFER,
  NYBLER_BINDING_TYPE_READ_ONLY_STORAGE_BUFFER,
  NYBLER_BINDING_TYPE_SAMPLER,
  NYBLER_BINDING_TYPE_COMPARISON_SAMPLER,
  NYBLER_BINDING_TYPE_TEXTURE,
  NYBLER_BINDING_TYPE_STORAGE_TEXTURE
} NyblerBindingType;

typedef enum NyblerPrimitiveTopology {
  NYBLER_PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
  NYBLER_PRIMITIVE_TOPOLOGY_LINE_LIST,
  NYBLER_PRIMITIVE_TOPOLOGY_LINE_STRIP,
  NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  NYBLER_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
} NyblerPrimitiveTopology;

typedef enum NyblerVertexFormat {
  NYBLER_VERTEX_FORMAT_FLOAT32 = 0,
  NYBLER_VERTEX_FORMAT_FLOAT32X2,
  NYBLER_VERTEX_FORMAT_FLOAT32X3,
  NYBLER_VERTEX_FORMAT_FLOAT32X4
} NyblerVertexFormat;

typedef struct NyblerVertexAttribute {
  uint32_t shader_location;
  uint32_t offset;
  NyblerVertexFormat format;
} NyblerVertexAttribute;

typedef enum NyblerLoadOp {
  NYBLER_LOAD_OP_LOAD = 0,
  NYBLER_LOAD_OP_CLEAR,
  NYBLER_LOAD_OP_DONT_CARE
} NyblerLoadOp;

typedef enum NyblerStoreOp {
  NYBLER_STORE_OP_STORE = 0,
  NYBLER_STORE_OP_DISCARD
} NyblerStoreOp;

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
typedef struct NyblerCommandBuffer NyblerCommandBuffer;
typedef struct NyblerRenderPassEncoder NyblerRenderPassEncoder;
typedef struct NyblerQueue NyblerQueue;
typedef struct NyblerLayout NyblerLayout;
typedef struct NyblerDisplayList NyblerDisplayList;
typedef struct NyblerPainter NyblerPainter;
typedef struct NyblerTextRenderer NyblerTextRenderer;

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

typedef struct NyblerExtent3D {
  uint32_t width;
  uint32_t height;
  uint32_t depth_or_array_layers;
} NyblerExtent3D;

typedef struct NyblerOrigin3D {
  uint32_t x;
  uint32_t y;
  uint32_t z;
} NyblerOrigin3D;

typedef struct NyblerBufferDescriptor {
  uint64_t size;
  uint32_t usage;
  const void *initial_data;
  const char *label;
} NyblerBufferDescriptor;

typedef struct NyblerTextureDescriptor {
  NyblerTextureFormat format;
  NyblerExtent3D size;
  uint32_t mip_level_count;
  uint32_t sample_count;
  uint32_t usage;
  const void *initial_data;
  const char *label;
} NyblerTextureDescriptor;

typedef struct NyblerSamplerDescriptor {
  NyblerAddressMode address_mode_u;
  NyblerAddressMode address_mode_v;
  NyblerAddressMode address_mode_w;
  NyblerFilterMode min_filter;
  NyblerFilterMode mag_filter;
  NyblerFilterMode mipmap_filter;
  NyblerCompareFunction compare;
  float lod_min_clamp;
  float lod_max_clamp;
  const char *label;
} NyblerSamplerDescriptor;

typedef struct NyblerBindGroupLayoutEntry {
  uint32_t binding;
  uint32_t visibility;
  NyblerBindingType binding_type;
  int has_dynamic_offset;
} NyblerBindGroupLayoutEntry;

typedef struct NyblerBindGroupLayoutDescriptor {
  const NyblerBindGroupLayoutEntry *entries;
  size_t entry_count;
  const char *label;
} NyblerBindGroupLayoutDescriptor;

typedef struct NyblerBindGroupEntry {
  uint32_t binding;
  NyblerBuffer *buffer;
  uint64_t offset;
  uint64_t size;
  NyblerTexture *texture;
  NyblerSampler *sampler;
} NyblerBindGroupEntry;

typedef struct NyblerBindGroupDescriptor {
  const NyblerBindGroupLayout *layout;
  const NyblerBindGroupEntry *entries;
  size_t entry_count;
  const char *label;
} NyblerBindGroupDescriptor;

typedef struct NyblerColorTargetState {
  NyblerTextureFormat format;
  int blend_enabled;
} NyblerColorTargetState;

typedef struct NyblerRenderPipelineDescriptor {
  NyblerShaderModule *vertex_module;
  NyblerShaderModule *fragment_module;
  const char *vertex_entry_point;
  const char *fragment_entry_point;
  const NyblerColorTargetState *targets;
  size_t target_count;
  NyblerPrimitiveTopology topology;
  const NyblerBindGroupLayout *layout;
  const NyblerVertexAttribute *vertex_attributes;
  size_t vertex_attribute_count;
  uint32_t vertex_stride;
  const char *label;
} NyblerRenderPipelineDescriptor;

typedef struct NyblerRenderPassColorAttachment {
  NyblerTexture *texture;
  NyblerColor clear_value;
  NyblerLoadOp load_op;
  NyblerStoreOp store_op;
} NyblerRenderPassColorAttachment;

typedef struct NyblerRenderPassDescriptor {
  const NyblerRenderPassColorAttachment *color_attachments;
  size_t color_attachment_count;
  const char *label;
} NyblerRenderPassDescriptor;

/* Readback uses WebGPU-shaped copy views. bytes_per_row must be at least the
 * packed row size; the backend packs each row into the destination buffer. */
typedef struct NyblerImageCopyTexture {
  NyblerTexture *texture;
  uint32_t mip_level;
  NyblerOrigin3D origin;
} NyblerImageCopyTexture;

typedef struct NyblerImageCopyBuffer {
  NyblerBuffer *buffer;
  uint64_t offset;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
} NyblerImageCopyBuffer;

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

/* Resource objects borrow their device. Release resources before the device. */
NYBLER_API NyblerResult nybler_device_create_buffer(
  NyblerDevice *device,
  const NyblerBufferDescriptor *descriptor,
  NyblerBuffer **out_buffer
);
NYBLER_API NyblerResult nybler_buffer_write(
  NyblerBuffer *buffer,
  uint64_t offset,
  const void *data,
  size_t size
);
NYBLER_API NyblerResult nybler_buffer_read(
  const NyblerBuffer *buffer,
  uint64_t offset,
  void *data,
  size_t size
);
NYBLER_API void nybler_buffer_release(NyblerBuffer *buffer);

NYBLER_API NyblerResult nybler_device_create_texture(
  NyblerDevice *device,
  const NyblerTextureDescriptor *descriptor,
  NyblerTexture **out_texture
);
NYBLER_API void nybler_texture_release(NyblerTexture *texture);

NYBLER_API NyblerResult nybler_device_create_sampler(
  NyblerDevice *device,
  const NyblerSamplerDescriptor *descriptor,
  NyblerSampler **out_sampler
);
NYBLER_API void nybler_sampler_release(NyblerSampler *sampler);

NYBLER_API NyblerResult nybler_device_create_bind_group_layout(
  NyblerDevice *device,
  const NyblerBindGroupLayoutDescriptor *descriptor,
  NyblerBindGroupLayout **out_layout
);
NYBLER_API void nybler_bind_group_layout_release(NyblerBindGroupLayout *layout);

NYBLER_API NyblerResult nybler_device_create_bind_group(
  NyblerDevice *device,
  const NyblerBindGroupDescriptor *descriptor,
  NyblerBindGroup **out_group
);
NYBLER_API void nybler_bind_group_release(NyblerBindGroup *group);

NYBLER_API NyblerResult nybler_device_create_render_pipeline(
  NyblerDevice *device,
  const NyblerRenderPipelineDescriptor *descriptor,
  NyblerRenderPipeline **out_pipeline
);
NYBLER_API void nybler_render_pipeline_release(NyblerRenderPipeline *pipeline);

/* A pass encoder borrows its command encoder; call nybler_render_pass_end()
 * before finishing or releasing the command encoder. The queue borrows the
 * device and is returned by nybler_device_get_queue(). */
NYBLER_API NyblerResult nybler_device_create_command_encoder(
  NyblerDevice *device,
  NyblerCommandEncoder **out_encoder
);
NYBLER_API NyblerResult nybler_command_encoder_begin_render_pass(
  NyblerCommandEncoder *encoder,
  const NyblerRenderPassDescriptor *descriptor,
  NyblerRenderPassEncoder **out_pass
);
NYBLER_API void nybler_render_pass_set_pipeline(
  NyblerRenderPassEncoder *pass,
  NyblerRenderPipeline *pipeline
);
NYBLER_API void nybler_render_pass_set_bind_group(
  NyblerRenderPassEncoder *pass,
  uint32_t index,
  NyblerBindGroup *group
);
NYBLER_API void nybler_render_pass_set_vertex_buffer(
  NyblerRenderPassEncoder *pass,
  uint32_t slot,
  NyblerBuffer *buffer,
  uint64_t offset
);
NYBLER_API void nybler_render_pass_draw(
  NyblerRenderPassEncoder *pass,
  uint32_t vertex_count,
  uint32_t instance_count,
  uint32_t first_vertex,
  uint32_t first_instance
);
NYBLER_API NyblerResult nybler_command_encoder_copy_texture_to_buffer(
  NyblerCommandEncoder *encoder,
  const NyblerImageCopyTexture *source,
  const NyblerImageCopyBuffer *destination,
  const NyblerExtent3D *copy_size
);
NYBLER_API void nybler_render_pass_end(NyblerRenderPassEncoder *pass);
NYBLER_API NyblerResult nybler_command_encoder_finish(
  NyblerCommandEncoder *encoder,
  NyblerCommandBuffer **out_commands
);
NYBLER_API void nybler_command_encoder_release(NyblerCommandEncoder *encoder);
NYBLER_API void nybler_command_buffer_release(NyblerCommandBuffer *commands);

NYBLER_API NyblerQueue *nybler_device_get_queue(NyblerDevice *device);
NYBLER_API NyblerResult nybler_queue_submit(
  NyblerQueue *queue,
  size_t command_count,
  NyblerCommandBuffer *const *commands
);

/* Display-list paint bridge. A painter owns the shaders, pipeline, and vertex
 * buffers needed to turn display-list boxes and text into colored quads.
 * nybler_painter_record() begins and ends the render pass itself, so the caller
 * can finish and submit the encoder. Release the painter only after the
 * submitted command buffers are no longer in use. */
NYBLER_API NyblerResult nybler_painter_create(
  NyblerDevice *device,
  NyblerPainter **out_painter
);
NYBLER_API NyblerResult nybler_painter_record(
  NyblerPainter *painter,
  NyblerCommandEncoder *encoder,
  const NyblerViewport *viewport,
  const NyblerDisplayList *display_list,
  const NyblerRenderPassDescriptor *render_pass
);
NYBLER_API void nybler_painter_release(NyblerPainter *painter);

/* Real glyph rendering backed by Trex (FreeType + HarfBuzz). A text renderer
 * owns a glyph atlas texture, sampler, bind group, and textured pipeline.
 * Attach one to a painter with nybler_painter_set_text_renderer(); after that,
 * TEXT display items are drawn as glyph quads instead of colored placeholders.
 * The painter borrows the renderer; keep it alive while commands are in use. */
NYBLER_API NyblerResult nybler_text_renderer_create(
  NyblerDevice *device,
  const char *font_path,
  float font_size,
  NyblerTextRenderer **out_renderer
);
NYBLER_API void nybler_text_renderer_release(NyblerTextRenderer *renderer);
NYBLER_API void nybler_painter_set_text_renderer(
  NyblerPainter *painter,
  NyblerTextRenderer *renderer
);

#ifdef __cplusplus
}
#endif

#endif /* NYBLER_NYBLER_H */
