# NybleJS - Lightweight JavaScript Engine

A from-scratch JavaScript engine written in C++17, optimized for performance. Implements a tree-walking interpreter with a custom lexer, recursive-descent parser, and value system.

## Features Implemented

### Lexer
- Full tokenizer: identifiers, numbers, strings, operators, keywords
- Line/column tracking for error reporting
- Single/multi-line comments

### Parser
- Recursive-descent parser with operator precedence
- Variable declarations: `let`, `const`, `var`
- Functions: declarations, arrow functions (`() => {}`)
- Control flow: `if/else`, `while`, `do-while`, `for`, `switch/case`
- `return`, `break`, `continue`
- Expressions: binary, unary, ternary, assignment with compound ops (`+=`, etc.)
- Member access: `obj.prop`, `obj[expr]`
- Call expressions
- Array and object literals
- Postfix `++`/`--`

### Interpreter
- Tree-walking execution
- Lexical scoping with closure support
- JavaScript type coercion rules
- Operators: arithmetic, comparison, strict/loose equality, logical
- String methods: `charAt`, `indexOf`, `slice`, `toUpperCase`, `toLowerCase`, `trim`, `startsWith`, `endsWith`, `includes`, `repeat`, `length`
- Array methods: `push`, `pop`, `shift`, `unshift`, `indexOf`, `includes`, `join`, `slice`, `splice`, `forEach`, `map`, `filter`, `reduce`, `find`, `some`, `every`, `sort`, `reverse`, `concat`, `length`

### Built-in APIs
- `console.log`, `.error`, `.warn`, `.time`, `.timeEnd`, `.assert`
- `Math` (all standard functions + constants)
- `JSON.parse`, `.stringify`
- `parseInt`, `parseFloat`, `isNaN`, `isFinite`
- `Object.keys`, `.values`, `.entries`, `.assign`
- `Array.isArray`, `.from`, `.of`
- `Number` (constants + `isNaN`, `isFinite`, `isInteger`, `isSafeInteger`)
- `String.fromCharCode`
- `Date.now`, `.parse`
- `Error`
- `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval`
- `encodeURI`/`decodeURI`/`encodeURIComponent`/`decodeURIComponent`

### Runtime
- REPL with multi-line input support
- File execution: `nyble script.js`

## Not Yet Implemented
- Prototype chain / class inheritance
- `new` operator for constructors
- `this` binding
- Generators / async / await
- `try/catch/finally`
- `Proxy`, `Reflect`, `Symbol`
- `Map`, `Set`, `WeakMap`, `WeakSet`
- `TypedArray`, `DataView`
- `Promise`
- `import`/`export` modules
- Garbage collection (reference counting only)
- JIT compilation
- `arguments` object
- Destructuring, spread/rest operators
- Template literals with expressions
- Regular expressions
- `Intl` APIs

## Building

Requirements: g++ with C++17 support (MinGW or MSYS2)

```bash
cd NybleJS
make
```

Or manually:
```bash
g++ -std=c++17 -O3 -flto src/main.cpp -o nyble
```

## Usage

REPL mode:
```bash
./nyble
```

Run a file:
```bash
./nyble script.js
```

## Performance

Built with aggressive optimization flags: `-O3 -flto -s`. Uses:
- Variant-based values (no virtual dispatch)
- Inline functions for arithmetic operations
- Header-only architecture for compiler inlining
- Direct C++ standard library integration

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
```
