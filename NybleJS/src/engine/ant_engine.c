#include "ant_engine_c.h"

// Ant's headers are C-only (void* -> const char* in C++ contexts), so this is
// the ONLY file in Nyble allowed to see them. The packaged embed dist and the
// submodule source tree name a few headers differently; NYBLE_ANT_PACKAGED is
// defined by the Makefile when building against tools/ant.
#include "ant.h" // source: include/ant.h | dist: include/ant.h (umbrella)

#ifdef NYBLE_ANT_PACKAGED
#include <ant/ant/runtime.h> // Ant_Create / Ant_Destroy / js_eval_bytecode_eval
#include <ant/bootstrap.h>
#include <ant/errors.h>       // Ant_Exception_Pending / Peek / Clear
#include <ant/isolate.h>     // js_glob
#include <ant/internal.h>    // js_to_number
#include <ant/reactor.h>
#include <ant/runtime.h>     // ant_runtime_init
#include <ant/silver/call.h> // sv_call_value
#else
#include <ant/runtime.h> // Ant_Create / Ant_Destroy / js_eval_bytecode_eval
#include <bootstrap.h>
#include <errors.h>
#include <isolate.h>
#include <internal.h>
#include <reactor.h>
#include <runtime.h> // ant_runtime_init
#include <silver/call.h>
#endif

#include <stdlib.h>
#include <string.h>

struct nyble_engine {
    ant_t *js;
    volatile char stack_base; // Ant scans the C stack from this address
    char argv0[8];
    char *argv[2];

    /* SpiderMonkey-style rooted value handles live on this private object in
     * the isolate global scope, so Ant's GC can see them. */
    ant_value_t roots;
    int32_t next_root;
    char *last_error;
};

static char *nyble_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *nyble_js_quote(const char *s) {
    size_t n = 0;
    for (const char *p = s; *p; p++) n += (*p == '\\' || *p == '"' || *p == '\n' || *p == '\r' || *p == '\t') ? 2 : 1;
    char *out = (char *)malloc(n + 3);
    if (!out) return NULL;
    char *w = out;
    *w++ = '"';
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '"') { *w++ = '\\'; *w++ = *p; }
        else if (*p == '\n') { *w++ = '\\'; *w++ = 'n'; }
        else if (*p == '\r') { *w++ = '\\'; *w++ = 'r'; }
        else if (*p == '\t') { *w++ = '\\'; *w++ = 't'; }
        else *w++ = *p;
    }
    *w++ = '"';
    *w = 0;
    return out;
}

nyble_engine *nyble_engine_create(int argc, char **argv, size_t heap_bytes, int node_compat) {
    nyble_engine *e = (nyble_engine *)calloc(1, sizeof(*e));
    if (!e) return NULL;

    memcpy(e->argv0, "nyble", 6);
    e->argv[0] = e->argv0;
    e->argv[1] = NULL;

    ant_isolate_options_t options;
    memset(&options, 0, sizeof(options));
    options.object_arena_bytes = heap_bytes;
    options.node_compat = node_compat ? true : false;

    e->js = Ant_Create(&options);
    if (!e->js) {
        free(e);
        return NULL;
    }

    js_setstackbase(e->js, (void *)&e->stack_base);

    if (argc <= 0 || !argv) {
        ant_runtime_init(e->js, 1, e->argv, NULL);
    } else {
        ant_runtime_init(e->js, argc, argv, NULL);
    }
    ant_bootstrap_modules(e->js);

    /* Always expose Nyble.props for CLI -D compat. */
    {
        static const char init[] = "globalThis.Nyble=globalThis.Nyble||{props:{}};";
        js_eval_bytecode_eval(e->js, init, sizeof(init) - 1, false);
    }

    /* Private root table for the nyble_value_t handles. */
    e->roots = Ant_Object_Create(e->js);
    js_set(e->js, js_glob(e->js), "__nyble_sm_roots", e->roots);
    e->next_root = 0;
    e->last_error = NULL;

    return e;
}

void nyble_engine_set_prop(nyble_engine *e, const char *key, const char *value) {
    if (!e || !e->js || !key || !value) return;
    char *k = nyble_js_quote(key);
    char *v = nyble_js_quote(value);
    if (!k || !v) { free(k); free(v); return; }
    /* globalThis.Nyble.props[<k>]=<v>; */
    size_t klen = strlen(k), vlen = strlen(v);
    const char prefix[] = "globalThis.Nyble=globalThis.Nyble||{props:{}};globalThis.Nyble.props[";
    const char mid[] = "]=";
    const char suffix[] = ";";
    size_t total = sizeof(prefix) - 1 + klen + sizeof(mid) - 1 + vlen + sizeof(suffix) - 1;
    char *buf = (char *)malloc(total + 1);
    if (buf) {
        memcpy(buf, prefix, sizeof(prefix) - 1);
        memcpy(buf + sizeof(prefix) - 1, k, klen);
        memcpy(buf + sizeof(prefix) - 1 + klen, mid, sizeof(mid) - 1);
        memcpy(buf + sizeof(prefix) - 1 + klen + sizeof(mid) - 1, v, vlen);
        memcpy(buf + total - 1, suffix, 1);
        buf[total] = 0;
        js_eval_bytecode_eval(e->js, buf, total, false);
        free(buf);
    }
    free(k);
    free(v);
}

void nyble_engine_destroy(nyble_engine *e) {
    if (!e) return;
    free(e->last_error);
    e->last_error = NULL;
    if (e->js) Ant_Destroy(e->js);
    free(e);
}

int nyble_engine_eval(nyble_engine *e, const char *src, size_t len, char **out) {
    if (!out) return 0;
    *out = NULL;
    if (!e || !e->js) {
        *out = nyble_strdup("Ant engine is not initialized");
        return 0;
    }
    if (!src) {
        src = "";
        len = 0;
    }

    ant_value_t v = js_eval_bytecode_eval(e->js, src, len, false);

    if (vtype(v) == kTypeError) {
        *out = nyble_strdup(js_str(e->js, v));
        /* Consume the pending throw so js_run_event_loop() cannot turn a
         * recoverable eval error into process exit. */
        Ant_Exception_Clear(e->js);
        return 0;
    }

    // REPL-style echo: describe() renders objects usefully; undefined/null
    // produce no line, matching the old NybleJS REPL.
    if (vtype(v) == kTypeUndefined || vtype(v) == kTypeNull) {
        *out = nyble_strdup("");
    } else {
        *out = nyble_strdup(js_str(e->js, Ant_Value_Describe(e->js, v)));
    }
    return 1;
}

void nyble_engine_drain(nyble_engine *e) {
    if (e && e->js) js_run_event_loop(e->js);
}

void *nyble_engine_isolate(nyble_engine *e) {
    return e ? e->js : NULL;
}

/* ---------------------------------------------------------------------- */
/* Rooted value handles for the SpiderMonkey-style embedding API.          */
/* ---------------------------------------------------------------------- */

static void nyble_set_error(nyble_engine *e, const char *msg) {
    if (!e) return;
    free(e->last_error);
    e->last_error = nyble_strdup(msg ? msg : "JavaScript error");
}

static void nyble_clear_error(nyble_engine *e) {
    if (!e) return;
    free(e->last_error);
    e->last_error = NULL;
}

const char *nyble_engine_last_error(nyble_engine *e) {
    return (e && e->last_error) ? e->last_error : "";
}

void nyble_engine_clear_exception(nyble_engine *e) {
    if (!e || !e->js) return;
    Ant_Exception_Clear(e->js);
    nyble_clear_error(e);
}

int nyble_engine_pending_exception(nyble_engine *e, char **out) {
    if (!out) return 0;
    *out = NULL;
    if (!e || !e->js || !Ant_Exception_Pending(e->js)) return 0;
    ant_value_t ex = Ant_Exception_Peek(e->js);
    const char *s = js_str(e->js, ex);
    *out = nyble_strdup(s);
    return 1;
}

static void nyble_root_key(nyble_value_t id, char *buf, size_t n) {
    snprintf(buf, n, "%d", (int)id);
}

static void nyble_root_put(nyble_engine *e, nyble_value_t id, ant_value_t v) {
    char key[32];
    nyble_root_key(id, key, sizeof(key));
    js_set(e->js, e->roots, key, v);
}

static ant_value_t nyble_root_get(nyble_engine *e, nyble_value_t id) {
    char key[32];
    nyble_root_key(id, key, sizeof(key));
    return js_get(e->js, e->roots, key);
}

static nyble_value_t nyble_root_value(nyble_engine *e, ant_value_t v) {
    if (!e || !e->js) return NYBLE_VALUE_INVALID;
    if (Ant_Exception_Pending(e->js)) {
        ant_value_t ex = Ant_Exception_Peek(e->js);
        nyble_set_error(e, js_str(e->js, ex));
        return NYBLE_VALUE_INVALID;
    }
    nyble_value_t id = ++e->next_root;
    if (id <= 0) { /* overflow: ids start at 1 */
        nyble_set_error(e, "Too many rooted values");
        return NYBLE_VALUE_INVALID;
    }
    nyble_root_put(e, id, v);
    return id;
}

nyble_value_t nyble_value_undefined(nyble_engine *e) {
    return e && e->js ? nyble_root_value(e, Ant_Value_Undefined) : NYBLE_VALUE_INVALID;
}

nyble_value_t nyble_value_null(nyble_engine *e) {
    return e && e->js ? nyble_root_value(e, Ant_Value_Null) : NYBLE_VALUE_INVALID;
}

nyble_value_t nyble_value_bool(nyble_engine *e, int value) {
    return e && e->js ? nyble_root_value(e, value ? Ant_Value_True : Ant_Value_False) : NYBLE_VALUE_INVALID;
}

nyble_value_t nyble_value_number(nyble_engine *e, double value) {
    return e && e->js ? nyble_root_value(e, Ant_Value_FromNumber(value)) : NYBLE_VALUE_INVALID;
}

nyble_value_t nyble_value_string(nyble_engine *e, const char *s, size_t len) {
    if (!e || !e->js) return NYBLE_VALUE_INVALID;
    if (!s) { s = ""; len = 0; }
    return nyble_root_value(e, js_mkstr(e->js, s, len));
}

nyble_value_t nyble_value_eval(nyble_engine *e, const char *src, size_t len) {
    nyble_clear_error(e);
    if (!e || !e->js || !src) {
        nyble_set_error(e, "Ant engine is not initialized");
        return NYBLE_VALUE_INVALID;
    }
    ant_value_t v = js_eval_bytecode_eval(e->js, src, len, false);
    if (Ant_Exception_Pending(e->js)) {
        ant_value_t ex = Ant_Exception_Peek(e->js);
        nyble_set_error(e, js_str(e->js, ex));
        return NYBLE_VALUE_INVALID;
    }
    if (vtype(v) == kTypeError) {
        nyble_set_error(e, js_str(e->js, v));
        return NYBLE_VALUE_INVALID;
    }
    return nyble_root_value(e, v);
}

int nyble_value_get_prop(nyble_engine *e, nyble_value_t obj, const char *key, nyble_value_t *out) {
    if (!out) return 0;
    *out = NYBLE_VALUE_INVALID;
    if (!e || !e->js || !key || obj <= 0) return 0;
    ant_value_t v = js_get(e->js, nyble_root_get(e, obj), key);
    if (Ant_Exception_Pending(e->js)) {
        ant_value_t ex = Ant_Exception_Peek(e->js);
        nyble_set_error(e, js_str(e->js, ex));
        return 0;
    }
    *out = nyble_root_value(e, v);
    return *out != NYBLE_VALUE_INVALID;
}

int nyble_value_set_prop(nyble_engine *e, nyble_value_t obj, const char *key, nyble_value_t val) {
    if (!e || !e->js || !key || obj <= 0 || val <= 0) return 0;
    js_set(e->js, nyble_root_get(e, obj), key, nyble_root_get(e, val));
    if (Ant_Exception_Pending(e->js)) {
        ant_value_t ex = Ant_Exception_Peek(e->js);
        nyble_set_error(e, js_str(e->js, ex));
        return 0;
    }
    return 1;
}

int nyble_value_call(nyble_engine *e, nyble_value_t fn, nyble_value_t this_val,
                     const nyble_value_t *args, size_t nargs, nyble_value_t *out) {
    if (!out) return 0;
    *out = NYBLE_VALUE_INVALID;
    if (!e || !e->js || fn <= 0 || (nargs > 0 && !args)) return 0;
    ant_value_t *argv = NULL;
    if (nargs > 0) {
        argv = (ant_value_t *)malloc(nargs * sizeof(*argv));
        if (!argv) { nyble_set_error(e, "Out of memory"); return 0; }
        for (size_t i = 0; i < nargs; i++) argv[i] = nyble_root_get(e, args[i]);
    }
    ant_value_t this_v = this_val > 0 ? nyble_root_get(e, this_val) : js_glob(e->js);
    ant_value_t result = sv_call_value(e->js, nyble_root_get(e, fn), this_v, argv, (int)nargs);
    free(argv);
    if (Ant_Exception_Pending(e->js)) {
        ant_value_t ex = Ant_Exception_Peek(e->js);
        nyble_set_error(e, js_str(e->js, ex));
        return 0;
    }
    *out = nyble_root_value(e, result);
    return *out != NYBLE_VALUE_INVALID;
}

char *nyble_value_to_string(nyble_engine *e, nyble_value_t value) {
    if (!e || !e->js || value <= 0) return nyble_strdup("");
    ant_value_t v = nyble_root_get(e, value);
    const char *s = js_str(e->js, Ant_Value_Describe(e->js, v));
    return nyble_strdup(s);
}

double nyble_value_to_number(nyble_engine *e, nyble_value_t value) {
    if (!e || !e->js || value <= 0) return 0.0;
    return js_to_number(e->js, nyble_root_get(e, value));
}

nyble_value_type_t nyble_value_type(nyble_engine *e, nyble_value_t value) {
    if (!e || !e->js || value <= 0) return NYBLE_T_UNDEFINED;
    switch (vtype(nyble_root_get(e, value))) {
        case kTypeUndefined: return NYBLE_T_UNDEFINED;
        case kTypeNull:      return NYBLE_T_NULL;
        case kTypeBool:      return NYBLE_T_BOOLEAN;
        case kTypeNumber:    return NYBLE_T_NUMBER;
        case kTypeString:    return NYBLE_T_STRING;
        case kTypeFunction:
        case kTypeBuiltin:   return NYBLE_T_FUNCTION;
        case kTypeSymbol:    return NYBLE_T_SYMBOL;
        case kTypeBigInt:    return NYBLE_T_BIGINT;
        case kTypeError:     return NYBLE_T_OTHER;
        default:             return NYBLE_T_OBJECT;
    }
}

void nyble_value_release(nyble_engine *e, nyble_value_t value) {
    if (!e || !e->js || value <= 0) return;
    char key[32];
    nyble_root_key(value, key, sizeof(key));
    js_set(e->js, e->roots, key, Ant_Value_Undefined);
}
