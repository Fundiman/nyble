# NybleJS - Lightweight JavaScript Engine v0.7 (AttentionIsAllYouNeed)

A from-scratch JavaScript engine written in C++17, optimized for performance. Implements a **hybrid architecture**: a tree-walking interpreter for small scripts and a bytecode VM (stack-based) for larger scripts (threshold: 300 AST nodes). Includes a custom lexer, recursive-descent parser, bytecode compiler, two execution engines, a mark-sweep garbage collector, and a full value system with JavaScript-style type coercion.

## Features Implemented

### Lexer
- Full tokenizer: identifiers, numbers (int, float, scientific), strings (single/double quotes)
- Escape sequences in strings: `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\'`
- Line/column tracking for error reporting
- Single-line (`//`) and multi-line (`/* */`) comments

### Parser
- Recursive-descent parser with full operator precedence (14 levels)
- Variable declarations: `let`, `const`, `var`
- Functions: declarations, anonymous functions, arrow functions (`() => {}`, `param => expr`, `(params) => expr`)
- Control flow: `if/else`, `while`, `do-while`, `for`, `for...in`, `for...of`, `switch/case/default`
- `return`, `break`, `continue`
- Expressions: binary arithmetic, comparison, strict/loose equality, logical (`&&`, `||`), bitwise (`&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`), unary (`!`, `-`, `+`, `typeof`, `++`, `--`), ternary (`? :`), assignment with compound ops (`=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`, `>>>=`)
- Member access: `obj.prop` and `obj[expr]`
- Call expressions
- Array literals with hole support
- Object literals
- Postfix `++`/`--`
- `throw`, `try/catch/finally`

### Prototype Chain & Class Inheritance
- Prototype-based inheritance with `__proto__` chain walking for property lookup
- Function prototype setup: each function gets a `.prototype` property with `.constructor`
- `new` operator: constructor calls with prototype linking
- `this` binding: auto-bound for regular function calls and method calls
- Arrow functions: inherit `this` from enclosing scope (no own `this` binding)
- `Function.prototype.call`, `apply`, `bind` for explicit `this` control
- Inheritance via prototype reassignment (`Dog.prototype = new Animal()`)
- Constructor return value handling (object return overrides instance)

### Hybrid Engine

**Tree-Walking Interpreter** (for scripts < 300 AST nodes):
- Lexical scoping with closure support
- JavaScript type coercion rules
- Signal-based control flow (return, break, continue, throw)
- `try/catch/finally` with proper rethrow and scoping

**Bytecode VM** (for scripts >= 300 AST nodes):
- ~60 opcode instruction set (stack-based)
- Bytecode compiler with jump patching for control flow
- Scope enter/exit for block scoping
- Exception handling with catch/finally offset patching
- Call stack with call frames
- Function creation with closure capture
- Arrow function support (separate `MAKE_ARROW_FUNCTION` opcode)
- `new` operator with prototype linking
- Method calls via `CALL_METHOD` opcode (proper `this` binding)

### Garbage Collector
- Mark-sweep collector
- Root tracing (environment chains, temporary roots)
- RAII pinning for GC safety
- Memory budget based on system RAM

### Value System
- Types: Null, Undefined, Boolean, Number, String, Object, Array, Function, NativeFunction
- JavaScript-style type coercion (`toString()`, `toNumber()`)
- Arithmetic operators (`+`, `-`, `*`, `/`, `%`, `**`)
- Bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`)
- Strict (`===`) and loose (`==`) equality
- Comparison operators (`<`, `>`, `<=`, `>=`)
- Property/index access (`getProperty`, `setProperty`, `getIndex`, `setIndex`)
- Unary operators (`!`, `-`, `+`, `typeof`, `++`, `--`)

### String Methods
`charAt`, `indexOf`, `slice`, `toUpperCase`, `toLowerCase`, `trim`, `startsWith`, `endsWith`, `includes`, `repeat`, `length`

### Array Methods
`push`, `pop`, `shift`, `unshift`, `indexOf`, `includes`, `join`, `slice`, `splice`, `forEach`, `map`, `filter`, `reduce`, `find`, `some`, `every`, `sort`, `reverse`, `concat`, `length`

### Built-in APIs
- **console**: `log`, `error`, `warn`, `time`, `timeEnd`, `assert`
- **Math**: All 8 constants (PI, E, LN2, LN10, LOG2E, LOG10E, SQRT2, SQRT1_2) + 27 functions (abs, floor, ceil, round, trunc, sqrt, cbrt, pow, exp, log, log2, log10, min, max, random, sin, cos, tan, asin, acos, atan, atan2, sign, hypot, clz32, imul, fround)
- **JSON**: `parse` (basic), `stringify` (uses `toString()`, no full serialization)
- **Global**: `parseInt`, `parseFloat`, `isNaN`, `isFinite`, `typeof`, `NaN`, `Infinity`, `undefined`, `globalThis`
- **Object**: `keys`, `values`, `entries`, `assign`, `create`, `defineProperty`
- **Function**: `prototype`, `call`, `apply`, `bind`
- **Array**: `isArray`, `from`, `of`
- **Number**: Constants (MAX_VALUE, MIN_VALUE, NaN, POSITIVE_INFINITY, NEGATIVE_INFINITY, MAX_SAFE_INTEGER, MIN_SAFE_INTEGER, EPSILON) + methods (`isNaN`, `isFinite`, `isInteger`, `isSafeInteger`)
- **String**: `fromCharCode`, `fromCodePoint`
- **Date**: `now`, `parse`, `UTC` (static) + constructor (`new Date()`, `new Date(ms)`, `new Date(str)`, `new Date(y,m,d,h,min,s,ms)`) + instance methods (`getTime`, `getFullYear`, `getMonth`, `getDate`, `getDay`, `getHours`, `getMinutes`, `getSeconds`, `getMilliseconds`, `getTimezoneOffset`, `getUTC*`, `setTime`, `setFullYear`, `setMonth`, `setDate`, `setHours`, `setMinutes`, `setSeconds`, `setMilliseconds`, `toString`, `toDateString`, `toTimeString`, `toISOString`, `toJSON`, `toUTCString`, `toLocaleString`, `toLocaleDateString`, `toLocaleTimeString`, `valueOf`)
- **Error**: `Error` constructor
- **Timer stubs**: `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval`
- **URI stubs**: `encodeURI`, `decodeURI`, `encodeURIComponent`, `decodeURIComponent`
- **eval stub**: Returns string representation

### Runtime
- REPL with syntax highlighting (as-you-type via raw terminal input)
- REPL keybindings: arrow keys, Home/End, Ctrl+A/E/U/K/W/D/C
- Multi-line input support (auto-detects incomplete input)
- ANSI colored output on all built-in messages (errors, help, version, REPL)
- Java-style CLI options (`-Xmx`, `-engine`, `-c`, `-Dkey=value`)
- `--meow` flag: ASCII cat with a random quote or cat fact
- Single-header include (`src/nyblejs.h`) for embedding as a library
- `nyble::Context`: V8-like embedding API — auto-selects tree/VM per script, roots the GC, keeps code alive across scripts, unifies JS exceptions (`src/context.h`)
- File execution: `njs script.js`
- Makefile build system (multi-file compilation)

## Not Yet Implemented
- `class` declarations (the keyword is lexed, but there is no parser/engine support — use prototype-based inheritance instead)
- Generators / async / await
- `Proxy`, `Reflect`, `Symbol`
- `Map`, `Set`, `WeakMap`, `WeakSet`
- `TypedArray`, `DataView`
- `Promise`
- `import`/`export` modules
- JIT compilation
- `arguments` object
- Destructuring, spread/rest operators
- Default parameters (`function f(x = 1)`)
- Object property shorthand (`{x, y}`)
- Template literals with expressions
- `String.prototype.split`
- Regular expressions
- `Intl` APIs
- `void`, `delete`, `instanceof` operators
- Comma operator
- Label statements
- `debugger`, `with` statements

## Building

Requirements: g++ with C++17 support (MinGW, MSYS2, or WSL)

```bash
make
```

Or manually:
```bash
g++ -std=c++17 -O3 -flto src/*.cpp -o njs
```

## Usage

REPL mode (with syntax highlighting):
```bash
./njs
```

Run a file:
```bash
./njs script.js
```

### Command-Line Options

| Option | Description |
|---|---|
| `-help`, `--help` | Show help message and exit |
| `-version`, `--version` | Show version info and exit |
| `-Xmx<size>` | Set GC max memory budget (e.g. `256m`, `1g`) |
| `-engine <type>`, `--engine <type>` | Force execution engine: `tree`, `vm`, or `jit` |
| `-c <code>` | Execute inline JavaScript code |
| `--meow` | Display an ASCII cat with a random quote or cat fact |
| `-D<key>=<value>` | Set a custom property |

Engine types:
- `tree` — Tree-walking interpreter only (fast startup, good for small scripts)
- `vm` — Bytecode VM only (better for large/complex scripts)
- `jit` — JIT compiler (not yet implemented, errors out)

Examples:
```bash
./njs -engine tree script.js
./njs -Xmx512m script.js
./njs --engine vm -Xmx1g script.js
./njs -Dname=myapp -Ddebug=true script.js
./njs -c "console.log(42)"
./njs --meow
```

### REPL Keybindings

| Key | Action |
|---|---|
| Left / Right | Move cursor |
| Home / End | Jump to start / end of line |
| Ctrl+A | Start of line |
| Ctrl+E | End of line |
| Ctrl+W | Delete word backward |
| Ctrl+U | Delete from cursor to start |
| Ctrl+K | Delete from cursor to end |
| Ctrl+C | Cancel current line |
| Ctrl+D | Exit REPL |

The REPL also works in **piped / non-interactive mode** (`echo "1 + 1" | njs` or `njs < script.js`): it reads lines from stdin, evaluates each, and prints results — no banner or prompts. Bare expressions echo their value (`69 > 67` → `true`), with an optional trailing semicolon; declarations (`let`, `function`) do not echo. Multi-line constructs (unbalanced `{`/`(`/`[`) continue reading until they close. Ctrl+D (EOF) ends a piped session.

### Embedding in Your Own App

NybleJS is embeddable as a library — your app is the host, NybleJS is the JS engine (like V8 in Chrome). Include the single header, then compile the engine sources (everything in `src/` except `main.cpp`, which defines the CLI/REPL) alongside your project, or link against the compiled object files:

```cpp
#include "src/nyblejs.h"
```

#### Recommended: `nyble::Context`

`Context` is the V8-style entry point: one call runs a script, it **auto-selects** between the tree-walking interpreter and the bytecode VM per script, roots the garbage collector for you, keeps code alive so functions persist across scripts, and unifies JS exceptions into `NybleRuntimeError`:

```cpp
#include <iostream>
#include "src/nyblejs.h"

int main() {
    // 1. One context = one shared global scope (like a V8 context/global object)
    nyble::Context ctx;

    // 2. Register a native C++ function — callable from JS immediately
    //    NativeFn = Value(const std::vector<Value>& args, const Value& thisVal)
    ctx.define("addCpp", [](const std::vector<nyble::Value>& a, const nyble::Value&) -> nyble::Value {
        return nyble::Value::makeNum(a[0].toNumber() + a[1].toNumber());
    });

    // 3. Run a script — engine picked automatically (tree < 300 AST nodes, VM otherwise)
    nyble::Value result;
    if (!ctx.evaluate("let x = addCpp(2, 3);", result)) {
        for (auto& err : ctx.parseErrors()) std::cerr << err << "\n";  // parse error
        return 1;
    }
    std::cout << result.toNumber() << "\n";  // 5

    // 4. State persists across scripts; JS errors come back as C++ exceptions
    ctx.evaluate("function double(n) { return n * 2; }", result);
    try {
        std::cout << ctx.call(ctx.get("double"), {nyble::Value::makeNum(21)}).toNumber() << "\n";  // 42
    } catch (const nyble::NybleRuntimeError& e) {
        std::cerr << "Uncaught: " << e.error.toString() << "\n";
    }
    return 0;
}
```

`Context` API: `evaluate(source, result)` (returns `false` on parse errors, throws `NybleRuntimeError` on uncaught JS errors), `define(name, value-or-native)`, `get(name)`, `call(fn, args, thisArg)`, `parseErrors()`, `setEngine(Context::Engine::Auto|Tree|VM)`, `setVmThreshold(n)`, `lastEngine()`, `globalEnv()`. `VM` and `Interpreter` are unchanged and still usable directly.

#### Manual engine control

If you want explicit control (or to embed both engines side by side), use `VM` or `Interpreter` directly:

```cpp
#include <iostream>
#include "src/nyblejs.h"

int main() {
    // 1. Engine instance (constructor installs console/Math/Date/etc. on globalEnv)
    nyble::VM vm;

    // 2. Root the GC at the engine (like V8 handles) — required
    nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
        if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
    };

    // 3. Register a native C++ function — now callable from JS
    vm.globalEnv->define("addCpp", nyble::Value::makeNative(
        [](const std::vector<nyble::Value>& a, const nyble::Value&) -> nyble::Value {
            return nyble::Value::makeNum(a[0].toNumber() + a[1].toNumber());
        }));

    // 4. Parse -> compile -> run
    std::string source = "console.log(addCpp(2, 3));";
    nyble::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    nyble::Parser parser(tokens);
    auto program = parser.parse();
    if (!parser.getErrors().empty()) { /* handle parse errors */ }

    nyble::BytecodeChunk chunk;
    nyble::Compiler comp(&chunk);
    comp.compile(program);

    try {
        vm.run(&chunk, vm.globalEnv);
    } catch (const nyble::VMThrow& t) {            // JS exception from the VM engine
        std::cerr << "Uncaught: " << t.value.toString() << "\n";
    } catch (const nyble::NybleRuntimeError& e) {  // JS exception thrown by a native
        std::cerr << "Uncaught: " << e.error.toString() << "\n";
    }
    return 0;
}
```

Notes for hosts:
- **Two engines**: `nyble::Interpreter` (tree-walker, best for small scripts) and `nyble::VM` (bytecode, best for large scripts). Both install builtins on their `globalEnv`. For the interpreter, catch `Interpreter::ThrowSignal` instead of `VMThrow`. `Context` auto-selects between them per script and routes calls across engines (a tree-defined function is callable from a VM script and vice versa).
- **Keeping code alive**: functions hold raw pointers into their parsed AST (tree engine) or bytecode chunk (VM). `Context` retains every script it runs, so cross-script functions stay valid. If you use `VM`/`Interpreter` directly, keep the `Program`/`BytecodeChunk` alive as long as any function value from it is in use.
- **Calling JS from C++**: `vm.callValue(fn, args, thisArg)` (`src/vm.h`), or `ctx.call(fn, args, thisArg)` — grab a JS function with `globalEnv->get("name")` / `ctx.get("name")` and call it back.
- **Throwing JS errors from natives**: `throw nyble::NybleRuntimeError(nyble::Value::makeTypeError("..."));` (also `makeReferenceError`, `makeRangeError`, `makeSyntaxError`).
- **`this` binding**: native methods registered on an object receive the receiver as the second `NativeFn` argument, so method-style calls work.
- **One engine per process**: `gHeap` and the global prototypes (`gObjectPrototype`, ...) are process-wide singletons (`src/value.h`, `src/gc.h`), so you cannot create multiple isolated contexts like Chrome's tabs — the main thing missing versus V8.
- **Registering native classes/types**: not supported yet. There is no `External`-style C++ pointer binding and the GC has no finalizer hooks, so only native *functions* can be registered. You can hand-roll a wrapper `GCObject` (stash a pointer in a property) but must manage the C++ object's lifetime yourself.

## Performance

Built with aggressive optimization flags: `-O3 -flto -s`. Uses:
- Variant-based values (no virtual dispatch)
- Hybrid architecture: tree-walking for small scripts, bytecode VM for large scripts
- Header/source split architecture for faster incremental builds
- Mark-sweep garbage collection
- No external dependencies (apart from C++ standard library)

## Example

```javascript
// Fibonacci
function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
console.log(fib(10));

// Array methods
let arr = [1, 2, 3, 4, 5];
console.log(arr.map(x => x * 2));

// Closure
function counter() {
    let count = 0;
    return function() {
        count = count + 1;
        return count;
    };
}
let c = counter();
console.log(c());
console.log(c());

// Try/catch
try {
    throw "error!";
} catch (e) {
    console.log(e);
} finally {
    console.log("done");
}

// Constructor + prototype + this binding
function Animal(name) {
    this.name = name;
}
Animal.prototype.speak = function() {
    return this.name + " says hi";
};

let a = new Animal("Rex");
console.log(a.speak());

// Inheritance
function Dog(name, breed) {
    this.name = name;
    this.breed = breed;
}
Dog.prototype = new Animal("");
Dog.prototype.bark = function() {
    return this.name + " barks!";
};

let d = new Dog("Buddy", "Golden");
console.log(d.speak());
console.log(d.bark());

// Function.prototype.call
function greet(greeting) {
    return greeting + ", " + this.name;
}
console.log(greet.call({name: "World"}, "Hello"));
```

## Contributing

### Versioning

NybleJS uses a two-level versioning system (major.minor). The version number is stored in `src/main.cpp` as `NYBLE_VERSION` and the version name as `NYBLE_VERSION_NAME`.

- **Major versions** (e.g. v0.4 → v1.0) are named. Names alternate between famous CS/AI research papers and foods from somewhere in the world — no sweets or desserts (to avoid sounding like an Android clone).
  - v0: "Attention Is All You Need" (paper)
  - v1: [food]
  - v2: [paper]
  - ...
- **Minor versions** (e.g. v0.4 → v0.5) add features, significant changes, or bug fixes. Minor versions have no name.

Every commit that changes engine behavior **must bump the minor version**.
