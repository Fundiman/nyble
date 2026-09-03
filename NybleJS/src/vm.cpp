#include "vm.h"

namespace nyble {

namespace {

// The temporary Interpreter used to evaluate tree-AST functions overwrites the
// global call dispatcher in its constructor; this restores it afterwards.
struct CallFnGuard {
    decltype(g_callFunction) saved;
    CallFnGuard() : saved(g_callFunction) {}
    ~CallFnGuard() { g_callFunction = saved; }
};

}

VM::VM() {
    globalEnv = std::make_shared<Environment>();
    installBuiltins(globalEnv);
}

Value VM::run(BytecodeChunk* chunk, std::shared_ptr<Environment> env) {
    CallFrame frame = {chunk, chunk->code.data(), env, {}, Value::makeUndefined()};
    std::vector<CallFrame> frames;
    frames.push_back(frame);

    while (!frames.empty()) {
        auto& cf = frames.back();

        // Handle pending throw
        if (isThrowing) {
            bool handled = false;
            while (!cf.tryStack.empty()) {
                auto& te = cf.tryStack.back();
                if (te.catchPC != 0) {
                    cf.env = te.savedEnv;
                    stack.resize(te.stackSize);
                    stack.push_back(throwValue);
                    cf.ip = cf.chunk->code.data() + te.catchPC;
                    isThrowing = false;
                    handled = true;
                    cf.tryStack.pop_back();
                    break;
                }
                if (te.finallyPC != 0) {
                    cf.env = te.savedEnv;
                    stack.resize(te.stackSize);
                    cf.ip = cf.chunk->code.data() + te.finallyPC;
                    cf.rethrowValue = throwValue;
                    isThrowing = false;
                    handled = true;
                    cf.tryStack.pop_back();
                    break;
                }
                cf.tryStack.pop_back();
            }
            if (!handled) {
                frames.pop_back();
                if (frames.empty()) {
                    stack.clear();
                    throw VMThrow{throwValue};
                }
                continue;
            }
            continue;
        }

        if ((size_t)(cf.ip - cf.chunk->code.data()) >= cf.chunk->code.size()) {
            frames.pop_back();
            if (frames.empty()) break;
            continue;
        }
        uint8_t instruction = *cf.ip++;

        try {
        switch (static_cast<OpCode>(instruction)) {
            case OpCode::NOP: break;

            case OpCode::PUSH_NULL:
                stack.push_back(Value::makeNull());
                break;

            case OpCode::PUSH_UNDEFINED:
                stack.push_back(Value::makeUndefined());
                break;

            case OpCode::PUSH_TRUE:
                stack.push_back(Value::makeBool(true));
                break;

            case OpCode::PUSH_FALSE:
                stack.push_back(Value::makeBool(false));
                break;

            case OpCode::PUSH_NUM: {
                uint16_t idx = readShort(cf);
                stack.push_back(Value::makeNum(cf.chunk->numConstants[idx]));
                break;
            }

            case OpCode::PUSH_STRING: {
                uint16_t idx = readShort(cf);
                stack.push_back(Value::makeStr(cf.chunk->strConstants[idx]));
                break;
            }

            case OpCode::POP:
                stack.pop_back();
                break;

            case OpCode::DUP:
                stack.push_back(stack.back());
                break;

            case OpCode::DUP2: {
                Value b = stack.back(); stack.pop_back();
                Value a = stack.back();
                stack.push_back(b);
                stack.push_back(a);
                stack.push_back(b);
                break;
            }

            case OpCode::SWAP: {
                Value a = stack.back(); stack.pop_back();
                Value b = stack.back(); stack.pop_back();
                stack.push_back(a); stack.push_back(b);
                break;
            }

            case OpCode::LOAD: {
                uint16_t idx = readShort(cf);
                const std::string& name = cf.chunk->strConstants[idx];
                stack.push_back(cf.env->get(name));
                break;
            }

            case OpCode::STORE: {
                uint16_t idx = readShort(cf);
                const std::string& name = cf.chunk->strConstants[idx];
                Value v = stack.back(); stack.pop_back();
                if (!cf.env->exists(name)) cf.env->define(name, Value::makeUndefined());
                cf.env->set(name, v);
                break;
            }

            case OpCode::GET_PROP: {
                uint16_t idx = readShort(cf);
                Value obj = stack.back(); stack.pop_back();
                stack.push_back(getProperty(obj, cf.chunk->strConstants[idx]));
                break;
            }

            case OpCode::SET_PROP: {
                uint16_t idx = readShort(cf);
                Value val = stack.back(); stack.pop_back();
                if (stack.back().type == ValueType::Object || stack.back().type == ValueType::Array)
                    stack.back().setProperty(cf.chunk->strConstants[idx], val);
                stack.push_back(val);
                break;
            }

            case OpCode::GET_INDEX: {
                Value index = stack.back(); stack.pop_back();
                Value obj = stack.back(); stack.pop_back();
                if (obj.type == ValueType::Object || obj.type == ValueType::Function) {
                    stack.push_back(obj.getProperty(index.toString()));
                } else if (obj.type == ValueType::Array) {
                    if (index.isNumber()) {
                        stack.push_back(obj.getIndex((size_t)index.toNumber()));
                    } else {
                        std::string key = index.toString();
                        char* end = nullptr;
                        double idx = std::strtod(key.c_str(), &end);
                        if (end == key.c_str() + key.size())
                            stack.push_back(obj.getIndex((size_t)idx));
                        else
                            stack.push_back(obj.getProperty(key));
                    }
                } else if (obj.type == ValueType::String) {
                    stack.push_back(obj.getIndex((size_t)index.toNumber()));
                } else {
                    stack.push_back(Value::makeUndefined());
                }
                break;
            }

            case OpCode::SET_INDEX: {
                Value val = stack.back(); stack.pop_back();
                Value index = stack.back(); stack.pop_back();
                Value obj = stack.back(); stack.pop_back();
                if (obj.type == ValueType::Array && index.isNumber()) {
                    obj.setIndex((size_t)index.toNumber(), val);
                } else if (obj.type == ValueType::Object || obj.type == ValueType::Function || obj.type == ValueType::Array) {
                    obj.setProperty(index.toString(), val);
                }
                stack.push_back(val);
                break;
            }

            case OpCode::NEW_OBJECT: {
                Value obj = Value::makeObj();
                if (gObjectPrototype) obj.objVal->proto = gObjectPrototype;
                stack.push_back(obj);
                break;
            }

            case OpCode::NEW_ARRAY: {
                uint16_t count = readShort(cf);
                Value arr = Value::makeArr();
                arr.arrVal->elements.reserve(count);
                size_t start = stack.size() - count;
                for (uint16_t i = 0; i < count; i++)
                    arr.arrVal->elements.push_back(stack[start + i]);
                stack.resize(start);
                stack.push_back(arr);
                break;
            }

            case OpCode::NEGATE: {
                Value v = stack.back(); stack.pop_back();
                stack.push_back(v.negate());
                break;
            }

            case OpCode::NOT: {
                Value v = stack.back(); stack.pop_back();
                stack.push_back(v.logicalNot());
                break;
            }

            case OpCode::TYPEOF: {
                Value v = stack.back(); stack.pop_back();
                stack.push_back(v.typeOf());
                break;
            }

            case OpCode::BIT_NOT: {
                Value v = stack.back(); stack.pop_back();
                stack.push_back(v.bitNot());
                break;
            }

            case OpCode::ADD: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.add(b)); break;
            }
            case OpCode::SUB: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.sub(b)); break;
            }
            case OpCode::MUL: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.mul(b)); break;
            }
            case OpCode::DIV: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.div(b)); break;
            }
            case OpCode::MOD: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.mod(b)); break;
            }
            case OpCode::POW: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.poww(b)); break;
            }
            case OpCode::BIT_AND: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.bitAnd(b)); break;
            }
            case OpCode::BIT_OR: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.bitOr(b)); break;
            }
            case OpCode::BIT_XOR: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.bitXor(b)); break;
            }
            case OpCode::SHL: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.shl(b)); break;
            }
            case OpCode::SHR: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.shr(b)); break;
            }
            case OpCode::USHR: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.ushr(b)); break;
            }
            case OpCode::EQ: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.eq(b, false)); break;
            }
            case OpCode::NEQ: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                Value r = a.eq(b, false); stack.push_back(Value::makeBool(!r.boolVal)); break;
            }
            case OpCode::STRICT_EQ: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.eq(b, true)); break;
            }
            case OpCode::STRICT_NEQ: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                Value r = a.eq(b, true); stack.push_back(Value::makeBool(!r.boolVal)); break;
            }
            case OpCode::LT: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.cmp(b, "<")); break;
            }
            case OpCode::GT: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.cmp(b, ">")); break;
            }
            case OpCode::LTE: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.cmp(b, "<=")); break;
            }
            case OpCode::GTE: {
                Value b = stack.back(); stack.pop_back(); Value a = stack.back(); stack.pop_back();
                stack.push_back(a.cmp(b, ">=")); break;
            }

            case OpCode::JMP: {
                int16_t offset = (int16_t)readShort(cf);
                cf.ip += offset;
                break;
            }

            case OpCode::JMP_IF_FALSE: {
                int16_t offset = (int16_t)readShort(cf);
                if (!stack.back().isTruthy()) cf.ip += offset;
                break;
            }

            case OpCode::JMP_IF_TRUE: {
                int16_t offset = (int16_t)readShort(cf);
                if (stack.back().isTruthy()) cf.ip += offset;
                break;
            }

            case OpCode::LOOP: {
                uint16_t offset = readShort(cf);
                cf.ip -= offset;
                break;
            }

            case OpCode::CALL: {
                uint16_t argCount = readShort(cf);
                size_t base = stack.size() - argCount;
                Value thisArg = stack[base - 1];
                Value callee = stack[base - 2];

                std::vector<Value> args(argCount);
                for (uint16_t i = 0; i < argCount; i++) args[i] = stack[base + i];
                stack.resize(base - 2);

                if (callee.type == ValueType::NativeFunction) {
                    try {
                        stack.push_back(callee.nativeVal(args, thisArg));
                    } catch (const VMThrow& vt) {
                        stack.push_back(vt.value);
                        isThrowing = true;
                        throwValue = vt.value;
                    }
                } else if (callee.type == ValueType::Function) {
                    auto funcData = callee.funcVal;
                    auto newEnv = funcData->closure->createChild();
                    if (!funcData->isArrow) {
                        newEnv->define("this", thisArg);
                    }
                    for (size_t i = 0; i < funcData->params.size(); i++)
                        newEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());

                    if (funcData->chunk) {
                        auto savedStack = stack;
                        stack.clear();
                        try {
                            Value result = run(funcData->chunk, newEnv);
                            stack = savedStack;
                            stack.push_back(result);
                        } catch (const VMThrow& vt) {
                            stack = savedStack;
                            stack.push_back(vt.value);
                            isThrowing = true;
                            throwValue = vt.value;
                        }
                    } else {
                        stack.clear();
                        CallFnGuard _guard;
                        Interpreter sub;
                        sub.currentEnv = newEnv;
                        Value result;
                        try {
                            if (funcData->exprBody) result = sub.evaluateExpr(funcData->exprBody);
                            else if (funcData->body) {
                                for (const auto& s : funcData->body->stmts) result = sub.execute(s.get());
                            }
                        } catch (const Interpreter::ReturnSignal& ret) { result = ret.value; }
                        catch (const Interpreter::ThrowSignal& ts) {
                            stack.push_back(ts.value);
                            isThrowing = true;
                            throwValue = ts.value;
                            break;
                        }
                        stack.push_back(result);
                    }
                } else {
                    isThrowing = true;
                    throwValue = Value::makeTypeError(callee.toString() + " is not a function");
                }
                break;
            }

            case OpCode::CALL_METHOD: {
                uint16_t argCount = readShort(cf);
                size_t base = stack.size() - argCount;
                Value thisArg = stack[base - 2];
                Value method = stack[base - 1];

                std::vector<Value> args(argCount);
                for (uint16_t i = 0; i < argCount; i++) args[i] = stack[base + i];
                stack.resize(base - 2);

                if (method.type == ValueType::NativeFunction) {
                    try {
                        stack.push_back(method.nativeVal(args, thisArg));
                    } catch (const VMThrow& vt) {
                        stack.push_back(vt.value);
                        isThrowing = true;
                        throwValue = vt.value;
                    }
                } else if (method.type == ValueType::Function) {
                    auto funcData = method.funcVal;
                    auto newEnv = funcData->closure->createChild();
                    if (!funcData->isArrow) {
                        newEnv->define("this", thisArg);
                    }
                    for (size_t i = 0; i < funcData->params.size(); i++)
                        newEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());

                    if (funcData->chunk) {
                        auto savedStack = stack;
                        stack.clear();
                        try {
                            Value result = run(funcData->chunk, newEnv);
                            stack = savedStack;
                            stack.push_back(result);
                        } catch (const VMThrow& vt) {
                            stack = savedStack;
                            stack.push_back(vt.value);
                            isThrowing = true;
                            throwValue = vt.value;
                        }
                    } else {
                        stack.clear();
                        CallFnGuard _guard;
                        Interpreter sub;
                        sub.currentEnv = newEnv;
                        Value result;
                        try {
                            if (funcData->exprBody) result = sub.evaluateExpr(funcData->exprBody);
                            else if (funcData->body) {
                                for (const auto& s : funcData->body->stmts) result = sub.execute(s.get());
                            }
                        } catch (const Interpreter::ReturnSignal& ret) { result = ret.value; }
                        catch (const Interpreter::ThrowSignal& ts) {
                            stack.push_back(ts.value);
                            isThrowing = true;
                            throwValue = ts.value;
                            break;
                        }
                        stack.push_back(result);
                    }
                } else {
                    isThrowing = true;
                    throwValue = Value::makeTypeError(method.toString() + " is not a function");
                }
                break;
            }

            case OpCode::RETURN: {
                Value result = stack.back(); stack.pop_back();
                frames.pop_back();
                if (!frames.empty()) stack.push_back(result);
                else return result;
                break;
            }

            case OpCode::MAKE_FUNCTION: {
                uint16_t idx = readShort(cf);
                auto funcData = gHeap.allocate<GCFunction>();
                funcData->closure = cf.env;
                auto fchunk = cf.chunk->functions[idx];
                funcData->chunk = fchunk;
                funcData->params = fchunk->params;
                Value proto = Value::makeObj();
                if (gObjectPrototype) proto.objVal->proto = gObjectPrototype;
                proto.objVal->properties["constructor"] = Value::makeFunc(funcData);
                funcData->properties["prototype"] = proto;
                stack.push_back(Value::makeFunc(funcData));
                break;
            }

            case OpCode::MAKE_ARROW_FUNCTION: {
                uint16_t idx = readShort(cf);
                auto funcData = gHeap.allocate<GCFunction>();
                funcData->closure = cf.env;
                funcData->isArrow = true;
                auto fchunk = cf.chunk->functions[idx];
                funcData->chunk = fchunk;
                funcData->params = fchunk->params;
                stack.push_back(Value::makeFunc(funcData));
                break;
            }

            case OpCode::SCOPE_ENTER:
                cf.env = cf.env->createChild();
                break;

            case OpCode::SCOPE_EXIT:
                if (cf.env->parent) cf.env = cf.env->parent;
                break;

            case OpCode::THROW: {
                Value val = stack.back(); stack.pop_back();
                isThrowing = true;
                throwValue = val;
                break;
            }

            case OpCode::RETHROW: {
                isThrowing = true;
                throwValue = cf.rethrowValue;
                cf.rethrowValue = Value::makeUndefined();
                break;
            }

            case OpCode::PUSH_TRY: {
                uint16_t catchOff = readShort(cf);
                uint16_t finallyOff = readShort(cf);
                TryEntry te;
                te.catchPC = catchOff;
                te.finallyPC = finallyOff;
                te.savedEnv = cf.env;
                te.stackSize = stack.size();
                cf.tryStack.push_back(te);
                break;
            }

            case OpCode::POP_TRY: {
                if (!cf.tryStack.empty()) cf.tryStack.pop_back();
                break;
            }

            case OpCode::NEW: {
                uint16_t argCount = readShort(cf);
                size_t base = stack.size() - argCount;
                Value constructor = stack[base - 1];

                if (!constructor.isFunction()) {
                    isThrowing = true;
                    throwValue = Value::makeTypeError(constructor.toString() + " is not a constructor");
                    break;
                }

                std::vector<Value> args(argCount);
                for (uint16_t i = 0; i < argCount; i++) args[i] = stack[base + i];
                stack.resize(base - 1);

                Value obj = Value::makeObj();
                Value protoVal = constructor.getProperty("prototype");
                if (protoVal.type == ValueType::Object) {
                    obj.objVal->proto = protoVal.objVal;
                }

                Value result = callValue(constructor, args, obj);
                if (result.type == ValueType::Object || result.type == ValueType::Array || result.type == ValueType::Function) {
                    stack.push_back(result);
                } else {
                    stack.push_back(obj);
                }
                break;
            }

            case OpCode::HALT:
                frames.clear();
                break;

            default:
                break;
        }
        } catch (const NybleRuntimeError& e) {
            isThrowing = true;
            throwValue = e.error;
        }
    }

    if (!stack.empty()) { Value r = stack.back(); stack.clear(); return r; }
    return Value::makeUndefined();
}

Value VM::callValue(const Value& fn, const std::vector<Value>& args, Value thisArg) {
    if (fn.type == ValueType::NativeFunction) {
        try {
            return fn.nativeVal(args, thisArg);
        } catch (const NybleRuntimeError& e) {
            stack.push_back(e.error);
            isThrowing = true;
            throwValue = e.error;
            return Value::makeUndefined();
        }
    }
    if (fn.type == ValueType::Function) {
        auto funcData = fn.funcVal;
        auto newEnv = funcData->closure->createChild();
        if (!funcData->isArrow) {
            newEnv->define("this", thisArg);
        }
        for (size_t i = 0; i < funcData->params.size(); i++)
            newEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());

        if (funcData->chunk) {
            auto savedStack = stack;
            stack.clear();
            try {
                Value result = run(funcData->chunk, newEnv);
                stack = savedStack;
                return result;
            } catch (const VMThrow& vt) {
                stack = savedStack;
                stack.push_back(vt.value);
                isThrowing = true;
                throwValue = vt.value;
                return Value::makeUndefined();
            }
        }

        stack.clear();
        CallFnGuard _guard;
        Interpreter sub;
        sub.currentEnv = newEnv;
        Value result;
        try {
            if (funcData->exprBody) result = sub.evaluateExpr(funcData->exprBody);
            else if (funcData->body) {
                for (const auto& s : funcData->body->stmts) result = sub.execute(s.get());
            }
        } catch (const Interpreter::ReturnSignal& ret) { result = ret.value; }
        catch (const Interpreter::ThrowSignal& ts) {
            stack.push_back(ts.value);
            isThrowing = true;
            throwValue = ts.value;
            return Value::makeUndefined();
        }
        return result;
    }
    return Value::makeUndefined();
}

Value VM::getProperty(const Value& obj, const std::string& name) {
    if (obj.type == ValueType::Null || obj.type == ValueType::Undefined) {
        throw NybleRuntimeError(Value::makeTypeError("Cannot read properties of " + obj.toString() + " (reading '" + name + "')"));
    }
    if (obj.type == ValueType::String) {
        if (name == "length") return Value::makeNum((double)obj.strVal->str.size());
        if (name == "charAt")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&) -> Value {
                int i = a.empty()?0:(int)a[0].toNumber();
                if(i<0||i>=(int)str.size()) return Value::makeStr("");
                return Value::makeStr(std::string(1,str[i]));
            });
        if (name == "indexOf")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&) -> Value {
                auto p = str.find(a.empty()?"":a[0].toString());
                return Value::makeNum(p==std::string::npos?-1.0:(double)p);
            });
        if (name == "slice")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&) -> Value {
                int s = a.empty()?0:(int)a[0].toNumber();
                int e = a.size()<2?(int)str.size():(int)a[1].toNumber();
                if(s<0)s=std::max(0,(int)str.size()+s);
                if(e<0)e=std::max(0,(int)str.size()+e);
                if(s>=e||s>=(int)str.size())return Value::makeStr("");
                return Value::makeStr(str.substr(s,e-s));
            });
        if (name == "toUpperCase")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&, const Value&)->Value{
                std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::toupper); return Value::makeStr(r);});
        if (name == "toLowerCase")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&, const Value&)->Value{
                std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::tolower); return Value::makeStr(r);});
        if (name == "trim")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&, const Value&)->Value{
                std::string s=str; s.erase(0,s.find_first_not_of(" \t\n\r")); s.erase(s.find_last_not_of(" \t\n\r")+1); return Value::makeStr(s);});
        if (name == "startsWith")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&)->Value{
                return Value::makeBool(str.find(a.empty()?"":a[0].toString())==0);});
        if (name == "endsWith")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&)->Value{
                std::string s=a.empty()?"":a[0].toString(); return Value::makeBool(str.size()>=s.size()&&str.substr(str.size()-s.size())==s);});
        if (name == "includes")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&)->Value{
                return Value::makeBool(str.find(a.empty()?"":a[0].toString())!=std::string::npos);});
        if (name == "repeat")
            return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a, const Value&)->Value{
                int c=a.empty()?0:(int)a[0].toNumber(); if(c<=0)return Value::makeStr("");
                std::string r; r.reserve(str.size()*c); for(int i=0;i<c;i++)r+=str; return Value::makeStr(r);});
        return Value::makeUndefined();
    }

    if (obj.type == ValueType::Array) {
        if (name == "length") return Value::makeNum((double)obj.arrVal->elements.size());
        if (name == "push")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&) mutable->Value{
                for(const auto& v:a) { arrPtr->elements.push_back(v); } return Value::makeNum((double)arrPtr->elements.size());});
        if (name == "pop")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&, const Value&) mutable->Value{
                if(arrPtr->elements.empty()) { return Value::makeUndefined(); } Value v=arrPtr->elements.back(); arrPtr->elements.pop_back(); return v;});
        if (name == "shift")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&, const Value&) mutable->Value{
                if(arrPtr->elements.empty()) { return Value::makeUndefined(); } Value v=arrPtr->elements.front(); arrPtr->elements.erase(arrPtr->elements.begin()); return v;});
        if (name == "unshift")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&) mutable->Value{
                arrPtr->elements.insert(arrPtr->elements.begin(),a.begin(),a.end()); return Value::makeNum((double)arrPtr->elements.size());});
        if (name == "indexOf")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty())return Value::makeNum(-1);
                for(size_t i=0;i<arrPtr->elements.size();i++){Value eq=arrPtr->elements[i].eq(a[0],true);if(eq.boolVal)return Value::makeNum((double)i);}
                return Value::makeNum(-1);});
        if (name == "includes")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty())return Value::makeBool(false);
                for(const auto& e:arrPtr->elements){Value eq=e.eq(a[0],true);if(eq.boolVal)return Value::makeBool(true);}
                return Value::makeBool(false);});
        if (name == "join")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                std::string sep=a.empty()?",":a[0].toString(); std::string r;
                for(size_t i=0;i<arrPtr->elements.size();i++){if(i>0)r+=sep;r+=arrPtr->elements[i].toString();}
                return Value::makeStr(r);});
        if (name == "slice")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                int s=a.empty()?0:(int)a[0].toNumber(); int e=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                if(s<0) { s=std::max(0,(int)arrPtr->elements.size()+s); } if(e<0) { e=std::max(0,(int)arrPtr->elements.size()+e); }
                Value r=Value::makeArr(); for(int i=s;i<e&&i<(int)arrPtr->elements.size();i++)r.arrVal->elements.push_back(arrPtr->elements[i]); return r;});
        if (name == "splice")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&) mutable->Value{
                int s=a.empty()?0:(int)a[0].toNumber(); int dc=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                if(s<0) { s=std::max(0,(int)arrPtr->elements.size()+s); } dc=std::min(dc,(int)arrPtr->elements.size()-s);
                Value rem=Value::makeArr(); auto it=arrPtr->elements.begin()+s;
                for(int i=0;i<dc;i++)rem.arrVal->elements.push_back(*(it+i));
                arrPtr->elements.erase(it,it+dc);
                for(size_t i=2;i<a.size();i++)arrPtr->elements.insert(arrPtr->elements.begin()+s+(i-2),a[i]);
                return rem;});
        if (name == "forEach")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                for(size_t i=0;i<arrPtr->elements.size();i++){
                    std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                    for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                    callValue(a[0], ca);}
                return Value::makeUndefined();});
        if (name == "map")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                for(size_t i=0;i<arrPtr->elements.size();i++){
                    std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                    for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                    r.arrVal->elements.push_back(callValue(a[0], ca));}
                return r;});
        if (name == "filter")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                for(size_t i=0;i<arrPtr->elements.size();i++){
                    std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                    for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                    Value rv=callValue(a[0], ca); if(rv.isTruthy())r.arrVal->elements.push_back(arrPtr->elements[i]);}
                return r;});
        if (name == "reduce")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                bool hasInit=a.size()>1; Value acc=hasInit?a[1]:Value::makeUndefined();
                size_t si=hasInit?0:1; if(!hasInit&&!arrPtr->elements.empty())acc=arrPtr->elements[0];
                for(size_t i=si;i<arrPtr->elements.size();i++)
                    acc=callValue(a[0], {acc,arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});
                return acc;});
        if (name == "find")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return arrPtr->elements[i];}
                return Value::makeUndefined();});
        if (name == "some")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return Value::makeBool(true);}
                return Value::makeBool(false);});
        if (name == "every")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(!r.isTruthy())return Value::makeBool(false);}
                return Value::makeBool(true);});
        if (name == "sort")
            return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                if(a.empty()||!a[0].isFunction())std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[](const Value& x,const Value& y){return x.toNumber()<y.toNumber();});
                else std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[this,&a](const Value& x,const Value& y){return callValue(a[0],{x,y}).toNumber()<0;});
                Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
        if (name == "reverse")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&, const Value&)->Value{
                std::reverse(arrPtr->elements.begin(),arrPtr->elements.end()); Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
        if (name == "concat")
            return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a, const Value&)->Value{
                Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements;
                for(const auto& v:a){if(v.isArray())r.arrVal->elements.insert(r.arrVal->elements.end(),v.arrVal->elements.begin(),v.arrVal->elements.end());else r.arrVal->elements.push_back(v);}
                return r;});
        return Value::makeUndefined();
    }

    if (obj.type == ValueType::Object) return obj.getProperty(name);
    if (obj.type == ValueType::Function || obj.type == ValueType::NativeFunction) return obj.getProperty(name);
    return Value::makeUndefined();
}

uint16_t VM::readShort(CallFrame& frame) {
    uint16_t b1 = *frame.ip++;
    uint16_t b2 = *frame.ip++;
    return b1 | (b2 << 8);
}

}
