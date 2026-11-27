#include "nyble_sm.h"

#include <stdlib.h>
#include <string.h>

struct NyRuntime {
    size_t heap_bytes;
    int node_compat;
    int argc;
    char **argv;
};

struct NyContext {
    NyRuntime *rt;
    nyble_engine *engine;
};

NyRuntime *Ny_NewRuntime(size_t heap_bytes, int node_compat) {
    NyRuntime *rt = (NyRuntime *)calloc(1, sizeof(*rt));
    if (!rt) return NULL;
    rt->heap_bytes = heap_bytes;
    rt->node_compat = node_compat ? 1 : 0;
    return rt;
}

void Ny_DestroyRuntime(NyRuntime *rt) {
    free(rt);
}

NyContext *Ny_NewContext(NyRuntime *rt, int argc, char **argv) {
    if (!rt) return NULL;
    NyContext *cx = (NyContext *)calloc(1, sizeof(*cx));
    if (!cx) return NULL;
    cx->rt = rt;
    cx->engine = nyble_engine_create(argc, argv, rt->heap_bytes, rt->node_compat);
    if (!cx->engine) {
        free(cx);
        return NULL;
    }
    return cx;
}

void Ny_DestroyContext(NyContext *cx) {
    if (!cx) return;
    nyble_engine_destroy(cx->engine);
    free(cx);
}

NyValue Ny_Undefined(NyContext *cx) {
    return cx ? nyble_value_undefined(cx->engine) : NY_VALUE_INVALID;
}

NyValue Ny_Null(NyContext *cx) {
    return cx ? nyble_value_null(cx->engine) : NY_VALUE_INVALID;
}

NyValue Ny_Boolean(NyContext *cx, int value) {
    return cx ? nyble_value_bool(cx->engine, value) : NY_VALUE_INVALID;
}

NyValue Ny_Number(NyContext *cx, double value) {
    return cx ? nyble_value_number(cx->engine, value) : NY_VALUE_INVALID;
}

NyValue Ny_String(NyContext *cx, const char *s, size_t len) {
    return cx ? nyble_value_string(cx->engine, s, len) : NY_VALUE_INVALID;
}

NyValue Ny_Evaluate(NyContext *cx, const char *src, size_t len) {
    if (!cx || !src) return NY_VALUE_INVALID;
    return nyble_value_eval(cx->engine, src, len);
}

int Ny_GetProperty(NyContext *cx, NyValue obj, const char *key, NyValue *out) {
    return cx && key ? nyble_value_get_prop(cx->engine, obj, key, out) : 0;
}

int Ny_SetProperty(NyContext *cx, NyValue obj, const char *key, NyValue val) {
    return cx && key ? nyble_value_set_prop(cx->engine, obj, key, val) : 0;
}

int Ny_Call(NyContext *cx, NyValue fn, NyValue this_val,
            const NyValue *args, size_t nargs, NyValue *out) {
    return cx ? nyble_value_call(cx->engine, fn, this_val, args, nargs, out) : 0;
}

char *Ny_ToString(NyContext *cx, NyValue value) {
    return cx ? nyble_value_to_string(cx->engine, value) : NULL;
}

double Ny_ToNumber(NyContext *cx, NyValue value) {
    return cx ? nyble_value_to_number(cx->engine, value) : 0.0;
}

NyType Ny_TypeOf(NyContext *cx, NyValue value) {
    return cx ? nyble_value_type(cx->engine, value) : NYBLE_T_UNDEFINED;
}

const char *Ny_GetErrorMessage(NyContext *cx) {
    return cx ? nyble_engine_last_error(cx->engine) : "";
}

int Ny_HasPendingException(NyContext *cx, char **out) {
    return cx ? nyble_engine_pending_exception(cx->engine, out) : 0;
}

void Ny_ClearPendingException(NyContext *cx) {
    if (cx) nyble_engine_clear_exception(cx->engine);
}

void Ny_DrainJobQueue(NyContext *cx) {
    if (cx) nyble_engine_drain(cx->engine);
}

void Ny_FreeValue(NyContext *cx, NyValue value) {
    if (cx) nyble_value_release(cx->engine, value);
}

const char *Ny_Version(void) {
    return "NybleJS SpiderMonkey-style embedding API 1.0";
}
