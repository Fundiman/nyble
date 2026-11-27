#ifndef NYBLE_SM_H
#define NYBLE_SM_H

/* SpiderMonkey-style embedding API for NybleJS (Ant Silver backend).
 *
 * This is a C ABI. Ant headers are C-only and never appear here; values are
 * opaque rooted handles (NyValue) backed by the ant_engine_c.h bridge.
 *
 * Naming follows SpiderMonkey's shape: a runtime, contexts, rooted values and
 * explicit operations (evaluate / get / set / call), rather than V8-style
 * handles, scopes and contexts-with-isolates.
 */

#include <stddef.h>
#include <stdint.h>

#include "ant_engine_c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NyRuntime NyRuntime;
typedef struct NyContext NyContext;
typedef nyble_value_t NyValue;
typedef nyble_value_type_t NyType;

#define NY_VALUE_INVALID NYBLE_VALUE_INVALID

/* Runtime: owns engine configuration (analogous to JSRuntime). */
NyRuntime *Ny_NewRuntime(size_t heap_bytes, int node_compat);
void Ny_DestroyRuntime(NyRuntime *rt);

/* Context: owns an Ant isolate / global scope (analogous to JSContext). */
NyContext *Ny_NewContext(NyRuntime *rt, int argc, char **argv);
void Ny_DestroyContext(NyContext *cx);

/* Value constructors return rooted values; release with Ny_FreeValue. */
NyValue Ny_Undefined(NyContext *cx);
NyValue Ny_Null(NyContext *cx);
NyValue Ny_Boolean(NyContext *cx, int value);
NyValue Ny_Number(NyContext *cx, double value);
NyValue Ny_String(NyContext *cx, const char *s, size_t len);

/* Evaluate source. Returns NY_VALUE_INVALID on a JS error; the message is
 * available from Ny_HasPendingException() / Ny_GetErrorMessage(). */
NyValue Ny_Evaluate(NyContext *cx, const char *src, size_t len);

int Ny_GetProperty(NyContext *cx, NyValue obj, const char *key, NyValue *out);
int Ny_SetProperty(NyContext *cx, NyValue obj, const char *key, NyValue val);
int Ny_Call(NyContext *cx, NyValue fn, NyValue this_val,
            const NyValue *args, size_t nargs, NyValue *out);

/* Display string (malloc'd, caller frees), numeric coercion and type tag. */
char *Ny_ToString(NyContext *cx, NyValue value);
double Ny_ToNumber(NyContext *cx, NyValue value);
NyType Ny_TypeOf(NyContext *cx, NyValue value);

/* Borrowed error string from the last operation ("" when none). */
const char *Ny_GetErrorMessage(NyContext *cx);

/* Pending exception (JS_GetPendingException equivalent). Returns 1 and sets
 * *out to a malloc'd message when an exception is pending. */
int Ny_HasPendingException(NyContext *cx, char **out);
void Ny_ClearPendingException(NyContext *cx);

/* Run pending promise jobs / timers (JS_JobQueue drain equivalent). */
void Ny_DrainJobQueue(NyContext *cx);

void Ny_FreeValue(NyContext *cx, NyValue value);
const char *Ny_Version(void);

#ifdef __cplusplus
}
#endif

#endif /* NYBLE_SM_H */
