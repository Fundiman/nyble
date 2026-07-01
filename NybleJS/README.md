# NybleJS - Lightweight JavaScript Engine v0.2.0

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
- REPL with multi-line input support (auto-detects incomplete input)
- File execution: `nyble script.js`
- Makefile build system (single compilation unit)

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
g++ -std=c++17 -O3 -flto src/main.cpp -o njs
```

## Usage

REPL mode:
```bash
./njs
```

Run a file:
```bash
./njs script.js
```

## Performance

Built with aggressive optimization flags: `-O3 -flto -s`. Uses:
- Variant-based values (no virtual dispatch)
- Hybrid architecture: tree-walking for small scripts, bytecode VM for large scripts
- Header-only architecture for compiler inlining
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
