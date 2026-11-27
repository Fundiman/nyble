#ifndef NYBLE_ANT_ENGINE_C_H
#define NYBLE_ANT_ENGINE_C_H

#include <stddef.h>
#include <stdint.h>

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

/* ----------------------------------------------------------------------
 * Rooted value handles used by the SpiderMonkey-style embedding API.
 * Ant values never cross this C ABI; hosts only see int32_t handles.
 * A handle is kept alive by storing the value on a private roots object in
 * the isolate's global scope (JS::RootedValue equivalent).
 * ---------------------------------------------------------------------- */

typedef int32_t nyble_value_t; /* -1 = invalid */
#define NYBLE_VALUE_INVALID ((nyble_value_t)-1)

typedef enum {
  NYBLE_T_UNDEFINED = 0,
  NYBLE_T_NULL,
  NYBLE_T_BOOLEAN,
  NYBLE_T_NUMBER,
  NYBLE_T_STRING,
  NYBLE_T_OBJECT,
  NYBLE_T_FUNCTION,
  NYBLE_T_SYMBOL,
  NYBLE_T_BIGINT,
  NYBLE_T_OTHER
} nyble_value_type_t;

nyble_value_t nyble_value_undefined(nyble_engine *e);
nyble_value_t nyble_value_null(nyble_engine *e);
nyble_value_t nyble_value_bool(nyble_engine *e, int value);
nyble_value_t nyble_value_number(nyble_engine *e, double value);
nyble_value_t nyble_value_string(nyble_engine *e, const char *s, size_t len);

/* Evaluate source and root the completion value. -1 on JS error; the error
 * text is available via nyble_engine_last_error(). */
nyble_value_t nyble_value_eval(nyble_engine *e, const char *src, size_t len);

int nyble_value_get_prop(nyble_engine *e, nyble_value_t obj, const char *key, nyble_value_t *out);
int nyble_value_set_prop(nyble_engine *e, nyble_value_t obj, const char *key, nyble_value_t val);
int nyble_value_call(nyble_engine *e, nyble_value_t fn, nyble_value_t this_val,
                     const nyble_value_t *args, size_t nargs, nyble_value_t *out);

/* Display string (malloc'd, caller frees) and numeric coercion. */
char *nyble_value_to_string(nyble_engine *e, nyble_value_t value);
double nyble_value_to_number(nyble_engine *e, nyble_value_t value);
nyble_value_type_t nyble_value_type(nyble_engine *e, nyble_value_t value);

/* Unroot a value handle. Safe to call on -1. */
void nyble_value_release(nyble_engine *e, nyble_value_t value);

/* Borrowed pointer to the last JS error text (empty string if none). */
const char *nyble_engine_last_error(nyble_engine *e);
int nyble_engine_pending_exception(nyble_engine *e, char **out);
void nyble_engine_clear_exception(nyble_engine *e);

#ifdef __cplusplus
}
#endif

#endif
