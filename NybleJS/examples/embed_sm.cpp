// SpiderMonkey-style embedding example (preferred v1 API).
// Runtime ~ JSRuntime, Context ~ JSContext, RootedValue ~ JS::RootedValue.

#include <iostream>
#include <vector>

#include "src/nyblejs.h"

int main() {
    nyble::sm::Runtime rt(0, /*nodeCompat=*/false);  // web-first default
    nyble::sm::Context cx(rt);
    if (!rt.valid() || !cx.valid()) return 1;

    // JS::RootedValue-ish: explicit, RAII-managed value handles.
    nyble::sm::RootedValue two = cx.evaluate("1 + 1");
    std::cout << two.toString() << '\n';  // 2

    nyble::sm::RootedValue greeting = cx.string("hello");
    nyble::sm::RootedValue upper = cx.evaluate("(s) => s.toUpperCase()");
    nyble::sm::RootedValue result = cx.call(upper, cx.undefined(), {greeting.get()});
    std::cout << result.toString() << '\n';  // "HELLO"

    // Global object property get/set, like JS_GetProperty/JS_SetProperty.
    nyble::sm::RootedValue global = cx.evaluate("globalThis");
    cx.setProperty(global, "answer", cx.number(42));
    std::cout << cx.getProperty(global, "answer").toString() << '\n';  // 42

    cx.drainJobQueue();
    return 0;
}
