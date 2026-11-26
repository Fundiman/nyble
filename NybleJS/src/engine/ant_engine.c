#include "ant_engine_c.h"

// Ant's headers are C-only (void* -> const char* in C++ contexts), so this is
// the ONLY file in Nyble allowed to see them. The packaged embed dist and the
// submodule source tree name a few headers differently; NYBLE_ANT_PACKAGED is
// defined by the Makefile when building against tools/ant.
#include "ant.h" // source: include/ant.h | dist: include/ant.h (umbrella)

#ifdef NYBLE_ANT_PACKAGED
#include <ant/ant/runtime.h> // Ant_Create / Ant_Destroy / js_eval_bytecode_eval
#include <ant/bootstrap.h>
#include <ant/isolate.h>
#include <ant/reactor.h>
#include <ant/runtime.h> // ant_runtime_init
#else
#include <ant/runtime.h> // Ant_Create / Ant_Destroy / js_eval_bytecode_eval
#include <bootstrap.h>
#include <isolate.h>
#include <reactor.h>
#include <runtime.h> // ant_runtime_init
#endif

#include <stdlib.h>
#include <string.h>

struct nyble_engine {
    ant_t *js;
    volatile char stack_base; // Ant scans the C stack from this address
    char argv0[8];
    char *argv[2];
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

nyble_engine *nyble_engine_create(int argc, char **argv, size_t heap_bytes) {
    nyble_engine *e = (nyble_engine *)calloc(1, sizeof(*e));
    if (!e) return NULL;

    memcpy(e->argv0, "nyble", 6);
    e->argv[0] = e->argv0;
    e->argv[1] = NULL;

    ant_isolate_options_t options;
    memset(&options, 0, sizeof(options));
    options.object_arena_bytes = heap_bytes;

    e->js = Ant_Create(heap_bytes ? &options : NULL);
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
        js_eval_bytecode_eval(e->js, init, sizeof(init) - 1);
    }

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
        js_eval_bytecode_eval(e->js, buf, total);
        free(buf);
    }
    free(k);
    free(v);
}

void nyble_engine_destroy(nyble_engine *e) {
    if (!e) return;
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
