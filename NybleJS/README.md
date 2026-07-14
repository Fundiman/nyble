# NybleJS - Lightweight JavaScript Engine v0.3.1 (AttentionIsAllYouNeed Biryani)

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
- Control flow: `if/else`, `while`, `do-while`, `for`, `switch/case/default`
- `return`, `break`, `continue`
- Expressions: binary arithmetic, comparison, strict/loose equality, logical (`&&`, `||`), unary (`!`, `-`, `+`, `typeof`, `++`, `--`), ternary (`? :`), assignment with compound ops (`=`, `+=`, `-=`, `*=`, `/=`, `%=`)
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
- ~50 opcode instruction set (stack-based)
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
- **Date**: `now`, `parse` (static methods only, no `new Date()` constructor or instance methods)
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
- File execution: `njs script.js`
- Makefile build system (multi-file compilation)

## Not Yet Implemented
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
- `new Date()` constructor and Date instance methods (`getFullYear`, `getMonth`, `getDate`, `toString`, etc.)
- `String.prototype.split`
- Regular expressions
- `Intl` APIs
- `void`, `delete`, `instanceof` operators
- Bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`, `>>>`)
- `for...in` / `for...of`
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

### Embedding as a Library

Include the single header for use in other C++ projects:
```cpp
#include "src/nyblejs.h"
```
Link against the compiled object files or compile `src/*.cpp` alongside your project.

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

NybleJS follows [Semantic Versioning](https://semver.org/). The version number is stored in `src/main.cpp` as `NYBLE_VERSION` and the version name as `NYBLE_VERSION_NAME`.

- **Major versions** (e.g. v0.3.0 → v1.0.0) are named after famous CS/AI research papers or landmark books.
  - Examples: "Attention Is All You Need", "The Cathedral and the Bazaar", "Structure and Interpretation of Computer Programs"
- **Minor versions** (e.g. v0.3.0 → v0.4.0) add features or significant changes. Minor versions receive a second name attached to the major name. This name must be a food from somewhere in the world — no sweets or desserts (to avoid sounding like an Android clone). If food names ever run out, animals are allowed as a fallback.
- **Patch versions** (e.g. v0.3.0 → v0.3.1) are bug fixes and small improvements.

Every commit that changes engine behavior **must bump the version by 0.0.1** (patch bump).
