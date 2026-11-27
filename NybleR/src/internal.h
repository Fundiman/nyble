#ifndef NYBLER_INTERNAL_H_
#define NYBLER_INTERNAL_H_

#include "nybler/nybler.h"

#include <stddef.h>

/* Private object layouts shared by the instance and resource implementations.
 * Every public object except a command buffer borrows its creating device. */

struct NyblerInstance {
  NyblerBackend backend;
  int headless;
  char *preferred_adapter;
};

struct NyblerAdapter {
  NyblerInstance *instance;
};

struct NyblerQueue {
  NyblerDevice *device;
};

struct NyblerDevice {
  NyblerAdapter *adapter;
  NyblerQueue *queue;
};

struct NyblerShaderModule {
  NyblerDevice *device;
  NyblerShaderFormat format;
  uint8_t *code;
  size_t code_size;
  char *label;
};

struct NyblerBuffer {
  NyblerDevice *device;
  uint64_t size;
  uint32_t usage;
  uint8_t *data;
  char *label;
};

struct NyblerTexture {
  NyblerDevice *device;
  NyblerTextureDescriptor descriptor;
  char *label;
};

struct NyblerSampler {
  NyblerDevice *device;
  NyblerSamplerDescriptor descriptor;
  char *label;
};

struct NyblerBindGroupLayout {
  NyblerDevice *device;
  NyblerBindGroupLayoutEntry *entries;
  size_t entry_count;
  char *label;
};

typedef struct NyblerBindGroupEntryStorage {
  uint32_t binding;
  NyblerBuffer *buffer;
  uint64_t offset;
  uint64_t size;
  NyblerTexture *texture;
  NyblerSampler *sampler;
  NyblerBindingType binding_type;
} NyblerBindGroupEntryStorage;

struct NyblerBindGroup {
  NyblerDevice *device;
  const NyblerBindGroupLayout *layout;
  NyblerBindGroupEntryStorage *entries;
  size_t entry_count;
  char *label;
};

struct NyblerRenderPipeline {
  NyblerDevice *device;
  NyblerShaderModule *vertex_module;
  NyblerShaderModule *fragment_module;
  char *vertex_entry_point;
  char *fragment_entry_point;
  NyblerColorTargetState *targets;
  size_t target_count;
  NyblerPrimitiveTopology topology;
  const NyblerBindGroupLayout *layout;
  char *label;
};

typedef enum NyblerCommandKind {
  NYBLER_COMMAND_NONE = 0,
  NYBLER_COMMAND_BEGIN_RENDER_PASS,
  NYBLER_COMMAND_END_RENDER_PASS,
  NYBLER_COMMAND_SET_PIPELINE,
  NYBLER_COMMAND_SET_BIND_GROUP,
  NYBLER_COMMAND_SET_VERTEX_BUFFER,
  NYBLER_COMMAND_DRAW
} NyblerCommandKind;

typedef struct NyblerCommand {
  NyblerCommandKind kind;
  union {
    struct {
      NyblerRenderPassColorAttachment *attachments;
      size_t attachment_count;
      char *label;
    } render_pass;
    struct {
      NyblerRenderPipeline *pipeline;
    } set_pipeline;
    struct {
      uint32_t index;
      NyblerBindGroup *group;
    } set_bind_group;
    struct {
      uint32_t slot;
      NyblerBuffer *buffer;
      uint64_t offset;
    } set_vertex_buffer;
    struct {
      uint32_t vertex_count;
      uint32_t instance_count;
      uint32_t first_vertex;
      uint32_t first_instance;
    } draw;
  } data;
} NyblerCommand;

struct NyblerRenderPassEncoder {
  NyblerCommandEncoder *encoder;
  int ended;
};

struct NyblerCommandEncoder {
  NyblerDevice *device;
  NyblerCommand *commands;
  size_t command_count;
  size_t command_capacity;
  NyblerRenderPassEncoder *active_pass;
};

struct NyblerCommandBuffer {
  NyblerDevice *device;
  NyblerCommand *commands;
  size_t command_count;
};

char *nybler_internal_duplicate_string(const char *value);

void nybler_internal_free_commands(NyblerCommand *commands, size_t count);

/* Computed style bridge. document.c owns Lexbor's CSSOM and exposes serialized
 * cascade winners to layout.c without leaking Lexbor types into the public API.
 * Returned strings are heap allocated and must be released with the matching
 * release function. */
char *nybler_internal_computed_style_value(
  NyblerDocument *document,
  NyblerNode *node,
  const char *property_name,
  size_t property_length,
  size_t *out_length
);

void nybler_internal_computed_style_value_release(char *value);

#endif /* NYBLER_INTERNAL_H_ */
