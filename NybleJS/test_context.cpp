#include <iostream>
#include "src/nyblejs.h"

int main() {
    int pass = 0, fail = 0;

    // Test 1: basic evaluate
    {
        nyble::Context ctx;
        nyble::Value r;
        bool ok = ctx.evaluate("let x = 10 + 20; x;", r);
        if (ok && r.toNumber() == 30) { std::cout << "PASS: basic evaluate\n"; pass++; }
        else { std::cout << "FAIL: basic evaluate (ok=" << ok << ")\n"; fail++; }
    }

    // Test 2: parse error returns false
    {
        nyble::Context ctx;
        nyble::Value r;
        bool ok = ctx.evaluate("let = ;", r);
        if (!ok && !ctx.parseErrors().empty()) { std::cout << "PASS: parse error\n"; pass++; }
        else { std::cout << "FAIL: parse error (ok=" << ok << ")\n"; fail++; }
    }

    // Test 3: define + get native
    {
        nyble::Context ctx;
        ctx.define("magic", nyble::Value::makeNum(42));
        nyble::Value r;
        bool ok = ctx.evaluate("magic;", r);
        if (ok && r.toNumber() == 42) { std::cout << "PASS: define + get native\n"; pass++; }
        else { std::cout << "FAIL: define + get native\n"; fail++; }
    }

    // Test 4: define native function
    {
        nyble::Context ctx;
        ctx.define("doubleIt", [](const std::vector<nyble::Value>& a, const nyble::Value&) -> nyble::Value {
            return nyble::Value::makeNum(a[0].toNumber() * 2);
        });
        nyble::Value r;
        bool ok = ctx.evaluate("doubleIt(21);", r);
        if (ok && r.toNumber() == 42) { std::cout << "PASS: native function\n"; pass++; }
        else { std::cout << "FAIL: native function\n"; fail++; }
    }

    // Test 5: cross-script state persistence
    {
        nyble::Context ctx;
        nyble::Value r;
        ctx.evaluate("function greet(name) { return 'hi ' + name; }", r);
        bool ok = ctx.evaluate("greet('world');", r);
        if (ok && r.toString() == "hi world") { std::cout << "PASS: cross-script persistence\n"; pass++; }
        else { std::cout << "FAIL: cross-script persistence\n"; fail++; }
    }

    // Test 6: call() from C++
    {
        nyble::Context ctx;
        nyble::Value r;
        ctx.evaluate("function add(a, b) { return a + b; }", r);
        nyble::Value fn = ctx.get("add");
        nyble::Value result = ctx.call(fn, {nyble::Value::makeNum(3), nyble::Value::makeNum(4)});
        if (result.toNumber() == 7) { std::cout << "PASS: call() from C++\n"; pass++; }
        else { std::cout << "FAIL: call() from C++\n"; fail++; }
    }

    // Test 7: JS exception -> NybleRuntimeError
    {
        nyble::Context ctx;
        nyble::Value r;
        ctx.evaluate("function oops() { throw 'kaboom'; }", r);
        nyble::Value fn = ctx.get("oops");
        bool threw = false;
        try {
            ctx.call(fn, {});
        } catch (const nyble::NybleRuntimeError& e) {
            threw = true;
            if (e.error.toString() == "kaboom") { std::cout << "PASS: JS exception propagation\n"; pass++; }
            else { std::cout << "FAIL: JS exception wrong value\n"; fail++; }
        }
        if (!threw) { std::cout << "FAIL: JS exception not thrown\n"; fail++; }
    }

    // Test 8: engine auto-selection
    {
        nyble::Context ctx;
        nyble::Value r;
        ctx.evaluate("let x = 1;", r);
        if (ctx.lastEngine() == nyble::Context::Engine::Tree) { std::cout << "PASS: auto-select tree\n"; pass++; }
        else { std::cout << "FAIL: auto-select tree\n"; fail++; }
    }

    // Test 9: engine forced VM
    {
        nyble::Context ctx;
        ctx.setEngine(nyble::Context::Engine::VM);
        nyble::Value r;
        ctx.evaluate("let x = 1;", r);
        if (ctx.lastEngine() == nyble::Context::Engine::VM) { std::cout << "PASS: forced VM\n"; pass++; }
        else { std::cout << "FAIL: forced VM\n"; fail++; }
    }

    // Test 10: prototype + new
    {
        nyble::Context ctx;
        nyble::Value r;
        bool ok = ctx.evaluate("function Dog(n) { this.name = n; } Dog.prototype.bark = function() { return this.name + '!'; }; let d = new Dog('Rex'); d.bark();", r);
        if (ok && r.toString() == "Rex!") { std::cout << "PASS: prototype + new\n"; pass++; }
        else { std::cout << "FAIL: prototype + new (ok=" << ok << " val=" << r.toString() << ")\n"; fail++; }
    }

    // Test 11: try/catch in VM mode
    // Verify the catch actually runs and can assign/read the caught value.
    // (The VM does not yet propagate a control-flow statement's inner result as
    // the program's return value the way the tree interpreter can, so we assert
    // on observable state rather than evaluate()'s return value.)
    {
        nyble::Context ctx;
        ctx.setEngine(nyble::Context::Engine::VM);
        nyble::Value r;
        bool ok = ctx.evaluate("let caught = null; try { throw 'err'; } catch(e) { caught = e; }", r);
        bool caught = ctx.get("caught").toString() == "err";
        if (ok && caught) { std::cout << "PASS: try/catch in VM\n"; pass++; }
        else { std::cout << "FAIL: try/catch in VM (ok=" << ok << " caught=" << caught << ")\n"; fail++; }
    }

    // Test 12: unhandled throw in VM -> NybleRuntimeError
    {
        nyble::Context ctx;
        ctx.setEngine(nyble::Context::Engine::VM);
        nyble::Value r;
        ctx.evaluate("function boom() { throw 'kaboom'; }", r);
        nyble::Value fn = ctx.get("boom");
        bool threw = false;
        try {
            ctx.call(fn, {});
        } catch (const nyble::NybleRuntimeError& e) {
            threw = true;
        }
        if (threw) { std::cout << "PASS: unhandled VM throw -> NybleRuntimeError\n"; pass++; }
        else { std::cout << "FAIL: unhandled VM throw not propagated\n"; fail++; }
    }

    // Test 13: unhandled throw in VM via evaluate -> NybleRuntimeError
    {
        nyble::Context ctx;
        ctx.setEngine(nyble::Context::Engine::VM);
        nyble::Value r;
        bool threw = false;
        try {
            ctx.evaluate("throw 'unhandled';", r);
        } catch (const nyble::NybleRuntimeError& e) {
            threw = true;
        }
        if (threw) { std::cout << "PASS: unhandled throw via evaluate -> NybleRuntimeError\n"; pass++; }
        else { std::cout << "FAIL: unhandled throw via evaluate not propagated\n"; fail++; }
    }

    // Test 14: cross-engine call (tree function from VM)
    {
        nyble::Context ctx;
        ctx.setEngine(nyble::Context::Engine::Tree);
        nyble::Value r;
        ctx.evaluate("function treeAdd(a, b) { return a + b; }", r);
        ctx.setEngine(nyble::Context::Engine::VM);
        ctx.evaluate("function vmCaller() { return treeAdd(10, 20); }", r);
        nyble::Value fn = ctx.get("vmCaller");
        nyble::Value result = ctx.call(fn, {});
        if (result.toNumber() == 30) { std::cout << "PASS: cross-engine call (tree from VM)\n"; pass++; }
        else { std::cout << "FAIL: cross-engine call\n"; fail++; }
    }

    std::cout << "\n" << pass << " passed, " << fail << " failed\n";
    return fail;
}
