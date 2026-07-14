#pragma once
#include <vector>
#include <stack>
#include <chrono>
#include <random>
#include "compiler.h"
#include "env.h"
#include "gc.h"
#include "builtins.h"
#include "interp.h"

namespace nyble {

struct TryEntry {
    size_t catchPC;
    size_t finallyPC;
    std::shared_ptr<Environment> savedEnv;
    size_t stackSize;
};

struct CallFrame {
    BytecodeChunk* chunk;
    uint8_t* ip;
    std::shared_ptr<Environment> env;
    std::vector<TryEntry> tryStack;
    Value rethrowValue;
};

struct VMThrow {
    Value value;
};

class VM {
public:
    std::shared_ptr<Environment> globalEnv;
    std::vector<Value> stack;
    bool isThrowing = false;
    Value throwValue;
    std::mt19937 rng{std::random_device{}()};

    VM();

    Value run(BytecodeChunk* chunk, std::shared_ptr<Environment> env);

    Value callValue(const Value& fn, const std::vector<Value>& args, Value thisArg = Value::makeUndefined());

    Value getProperty(const Value& obj, const std::string& name);

private:
    uint16_t readShort(CallFrame& frame);
};

}
