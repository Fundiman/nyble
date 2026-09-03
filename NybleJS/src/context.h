#pragma once
#include <string>
#include <vector>
#include <memory>
#include "vm.h"
#include "interp.h"

namespace nyble {

// A V8-like embedder context. Owns a shared global scope, automatically selects
// between the tree-walking Interpreter and the bytecode VM for every script
// (based on AST complexity), roots the garbage collector, and unifies JS
// exceptions into a single C++ exception type.
//
// Backwards compatible: VM and Interpreter are untouched and can still be used
// directly; Context just wraps them.
class Context {
public:
    enum class Engine { Auto, Tree, VM };

    explicit Context(Engine mode = Engine::Auto);
    ~Context();

    // Lex + parse + compile + run a script. Auto-selects the engine when in
    // Engine::Auto mode. Returns false (and fills parseErrors()) on a parse
    // error; throws NybleRuntimeError on an uncaught JS error.
    //
    // Parsed programs and compiled bytecode are retained for the Context's
    // lifetime, so functions defined in earlier scripts remain callable (tree
    // functions reference their AST, VM functions reference their chunk).
    bool evaluate(const std::string& source, Value& result);

    // Run a parsed program, taking ownership of it (so its functions stay
    // callable in later scripts).
    bool evaluate(Program&& prog, Value& result);

    // Run a parsed program you keep alive yourself. Functions defined in it
    // reference the program's AST/bytecode, so the program must outlive any
    // function values you keep. Prefer the source or rvalue overloads.
    bool evaluate(Program& prog, Value& result);

    // Convenience: run a script, discard the result.
    bool eval(const std::string& source) { Value r; return evaluate(source, r); }

    // Register a value or native C++ function on the shared global scope.
    void define(const std::string& name, Value value);
    void define(const std::string& name, NativeFn fn) { define(name, Value::makeNative(std::move(fn))); }

    Value get(const std::string& name) const { return globalEnv_ ? globalEnv_->get(name) : Value::makeUndefined(); }

    // Call a JS function from C++. Throws NybleRuntimeError if it throws.
    Value call(const Value& fn, const std::vector<Value>& args, Value thisArg = Value::makeUndefined());

    // Parse errors from the last evaluate() call that returned false.
    const std::vector<std::string>& parseErrors() const { return parseErrors_; }

    Engine engineMode() const { return mode_; }
    void setEngine(Engine mode) { mode_ = mode; }
    Engine lastEngine() const { return lastEngine_; }

    // Complexity threshold above which Engine::Auto uses the bytecode VM.
    size_t vmThreshold() const { return vmThreshold_; }
    void setVmThreshold(size_t n) { vmThreshold_ = n; }

    std::shared_ptr<Environment> globalEnv() const { return globalEnv_; }

private:
    std::shared_ptr<Environment> globalEnv_;
    std::unique_ptr<VM> vm_;
    std::unique_ptr<Interpreter> interp_;
    // Owns all code seen so far: tree functions reference their AST, VM
    // functions reference their bytecode chunk, so both must outlive the values.
    std::vector<std::unique_ptr<Program>> programs_;
    std::vector<std::unique_ptr<BytecodeChunk>> chunks_;
    Engine mode_;
    Engine lastEngine_;
    std::vector<std::string> parseErrors_;
    size_t vmThreshold_;

    void installCallDispatcher();
    bool runEngine(Program& prog, Value& result);
};

}
