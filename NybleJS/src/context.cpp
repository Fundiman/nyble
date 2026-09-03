#include "context.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "builtins.h"

namespace nyble {

Context::Context(Engine mode)
    : mode_(mode), lastEngine_(Engine::Auto), vmThreshold_(300) {
    // Create both engines first so their installBuiltins() calls run before the
    // shared scope's — the shared scope ends up owning the final prototypes.
    vm_ = std::make_unique<VM>();
    interp_ = std::make_unique<Interpreter>();

    globalEnv_ = std::make_shared<Environment>();
    installBuiltins(globalEnv_);

    interp_->globalEnv = globalEnv_;
    interp_->currentEnv = globalEnv_;
    vm_->globalEnv = globalEnv_;

    gHeap.rootTracer = [this](std::vector<GCHeader*>& wl) {
        if (globalEnv_) globalEnv_->traceGCValues(wl);
    };

    installCallDispatcher();
}

Context::~Context() {
    gHeap.rootTracer = nullptr;
    g_callFunction = nullptr;
}

void Context::installCallDispatcher() {
    g_callFunction = [this](const Value& fn, const std::vector<Value>& args, const Value& thisArg) -> Value {
        // VM-compiled functions can only be run by the VM; everything else
        // (tree functions, natives) runs in the Interpreter. The VM is already
        // able to fall back to a temporary Interpreter for tree functions.
        if (fn.type == ValueType::Function && fn.funcVal && fn.funcVal->chunk) {
            return vm_->callValue(fn, args, thisArg);
        }
        return interp_->callValue(fn, args, thisArg);
    };
}

bool Context::evaluate(const std::string& source, Value& result) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    parseErrors_ = parser.getErrors();
    if (!parseErrors_.empty()) return false;

    programs_.push_back(std::make_unique<Program>(std::move(program)));
    return runEngine(*programs_.back(), result);
}

bool Context::evaluate(Program&& prog, Value& result) {
    parseErrors_.clear();
    programs_.push_back(std::make_unique<Program>(std::move(prog)));
    return runEngine(*programs_.back(), result);
}

bool Context::evaluate(Program& prog, Value& result) {
    parseErrors_.clear();
    return runEngine(prog, result);
}

bool Context::runEngine(Program& prog, Value& result) {
    installCallDispatcher();

    Engine eng = mode_;
    if (eng == Engine::Auto) {
        size_t complexity = countAST(prog);
        eng = (complexity < vmThreshold_) ? Engine::Tree : Engine::VM;
    }
    lastEngine_ = eng;

    try {
        if (eng == Engine::Tree) {
            interp_->globalEnv = globalEnv_;
            interp_->currentEnv = globalEnv_;
            result = interp_->evaluate(prog);
        } else {
            vm_->isThrowing = false;
            vm_->throwValue = Value::makeUndefined();
            auto chunk = std::make_unique<BytecodeChunk>();
            Compiler comp(chunk.get());
            comp.compile(prog);
            result = vm_->run(chunk.get(), globalEnv_);
            if (vm_->isThrowing) {
                Value err = vm_->throwValue;
                vm_->isThrowing = false;
                vm_->throwValue = Value::makeUndefined();
                throw NybleRuntimeError(err);
            }
            chunks_.push_back(std::move(chunk));
        }
        return true;
    } catch (const VMThrow& vt) {
        throw NybleRuntimeError(vt.value);
    } catch (const Interpreter::ThrowSignal& ts) {
        throw NybleRuntimeError(ts.value);
    } catch (const NybleRuntimeError& e) {
        throw;
    }
}

void Context::define(const std::string& name, Value value) {
    if (!globalEnv_) return;
    globalEnv_->define(name, value);
}

Value Context::call(const Value& fn, const std::vector<Value>& args, Value thisArg) {
    installCallDispatcher();

    if (fn.type == ValueType::Function && fn.funcVal && fn.funcVal->chunk) {
        vm_->isThrowing = false;
        vm_->throwValue = Value::makeUndefined();
        Value r = vm_->callValue(fn, args, thisArg);
        if (vm_->isThrowing) {
            vm_->isThrowing = false;
            Value err = vm_->throwValue;
            vm_->throwValue = Value::makeUndefined();
            throw NybleRuntimeError(err);
        }
        return r;
    }

    try {
        return interp_->callValue(fn, args, thisArg);
    } catch (const Interpreter::ThrowSignal& ts) {
        throw NybleRuntimeError(ts.value);
    }
}

}
