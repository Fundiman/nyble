#ifndef NYBLE_ANT_ENGINE_C_H
#define NYBLE_ANT_ENGINE_C_H

#include <stddef.h>

/* C-side bridge to libant. Ant's headers are C-only, so nothing in here may be
 * included from C++ except this file (which is plain C ABI). */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nyble_engine nyble_engine;

nyble_engine *nyble_engine_create(int argc, char **argv, size_t heap_bytes, int node_compat);
void nyble_engine_destroy(nyble_engine *e);

/* Define a CLI -D property as Nyble.props.<key> = <value>. */
void nyble_engine_set_prop(nyble_engine *e, const char *key, const char *value);

/* Evaluate NUL-terminated `src`. Returns 1 on success and stores a malloc'd,
 * display-ready string in *out (never NULL; empty string for undefined/null).
 * Returns 0 on a JS error and stores the exception text in *out.
 * Caller frees *out. */
int nyble_engine_eval(nyble_engine *e, const char *src, size_t len, char **out);

void nyble_engine_drain(nyble_engine *e);

/* ant_t* — for host bindings that need the raw isolate. */
void *nyble_engine_isolate(nyble_engine *e);

#ifdef __cplusplus
}
#endif

#endif
