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
        ant_value_t global = js_glob(e->js);
        ant_value_t nyble = js_get(e->js, global, "Nyble");
        if (vtype(nyble) != kTypeObject) {
            nyble = Ant_Object_Create(e->js);
            js_setprop(e->js, global, js_mkstr(e->js, "Nyble", 5), nyble);
        }
        ant_value_t props = js_get(e->js, nyble, "props");
        if (vtype(props) != kTypeObject) {
            props = Ant_Object_Create(e->js);
            js_setprop(e->js, nyble, js_mkstr(e->js, "props", 5), props);
        }
    }

    return e;
}

void nyble_engine_set_prop(nyble_engine *e, const char *key, const char *value) {
    if (!e || !e->js || !key || !value) return;

    ant_value_t global = js_glob(e->js);
    ant_value_t nyble = js_get(e->js, global, "Nyble");
    if (vtype(nyble) != kTypeObject) {
        nyble = Ant_Object_Create(e->js);
        js_setprop(e->js, global, js_mkstr(e->js, "Nyble", 5), nyble);
    }

    ant_value_t props = js_get(e->js, nyble, "props");
    if (vtype(props) != kTypeObject) {
        props = Ant_Object_Create(e->js);
        js_setprop(e->js, nyble, js_mkstr(e->js, "props", 5), props);
    }

    js_set(e->js, props, key, js_mkstr(e->js, value, strlen(value)));
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
