#ifndef NYBLER_MESA_BACKEND_H_
#define NYBLER_MESA_BACKEND_H_

#include "internal.h"

/* Local Mesa GL backend. EGL/GLES entry points are resolved at runtime so the
 * stable nybler_* ABI does not link against a particular Mesa build. */
typedef struct NyblerMesaContext NyblerMesaContext;

NyblerResult nybler_mesa_init(NyblerMesaContext **out_context);

void nybler_mesa_destroy(NyblerMesaContext *context);

NyblerResult nybler_mesa_create_texture(
  NyblerMesaContext *context,
  NyblerTexture *texture
);
void nybler_mesa_destroy_texture(NyblerTexture *texture);

NyblerResult nybler_mesa_create_pipeline(
  NyblerMesaContext *context,
  NyblerRenderPipeline *pipeline
);
void nybler_mesa_destroy_pipeline(NyblerRenderPipeline *pipeline);

NyblerResult nybler_mesa_execute(
  NyblerMesaContext *context,
  size_t command_count,
  NyblerCommandBuffer *const *command_buffers
);

#endif /* NYBLER_MESA_BACKEND_H_ */
