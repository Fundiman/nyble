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

struct CallFrame {
    BytecodeChunk* chunk;
    uint8_t* ip;
    std::shared_ptr<Environment> env;
};

class VM {
public:
    std::shared_ptr<Environment> globalEnv;
    std::vector<Value> stack;
    std::mt19937 rng{std::random_device{}()};

    VM() {
        globalEnv = std::make_shared<Environment>();
        installBuiltins(globalEnv);
    }

    Value run(BytecodeChunk* chunk, std::shared_ptr<Environment> env) {
        CallFrame frame = {chunk, chunk->code.data(), env};
        std::vector<CallFrame> frames;
        frames.push_back(frame);

        while (!frames.empty()) {
            auto& cf = frames.back();
            if ((size_t)(cf.ip - cf.chunk->code.data()) >= cf.chunk->code.size()) {
                frames.pop_back();
                if (frames.empty()) break;
                continue;
            }
            uint8_t instruction = *cf.ip++;

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
                    if (!cf.env->exists(name)) cf.env->define(name, Value::makeUndefined());
                    cf.env->set(name, stack.back());
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
                    stack.push_back(obj.getIndex((size_t)index.toNumber()));
                    break;
                }

                case OpCode::SET_INDEX: {
                    Value val = stack.back(); stack.pop_back();
                    Value index = stack.back(); stack.pop_back();
                    Value obj = stack.back(); stack.pop_back();
                    obj.setIndex((size_t)index.toNumber(), val);
                    stack.push_back(val);
                    break;
                }

                case OpCode::NEW_OBJECT:
                    stack.push_back(Value::makeObj());
                    break;

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
                    Value callee = stack[stack.size() - argCount - 1];

                    if (callee.type == ValueType::NativeFunction) {
                        std::vector<Value> args(argCount);
                        size_t start = stack.size() - argCount;
                        for (uint16_t i = 0; i < argCount; i++) args[i] = stack[start + i];
                        stack.resize(start); stack.pop_back();
                        stack.push_back(callee.nativeVal(args));
                    } else if (callee.type == ValueType::Function) {
                        auto funcData = callee.funcVal;
                        std::vector<Value> args(argCount);
                        size_t start = stack.size() - argCount;
                        for (uint16_t i = 0; i < argCount; i++) args[i] = stack[start + i];
                        stack.resize(start); stack.pop_back();

                        auto newEnv = funcData->closure->createChild();
                        for (size_t i = 0; i < funcData->params.size(); i++)
                            newEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());

                        if (funcData->chunk) {
                            auto savedStack = stack;
                            stack.clear();
                            Value result = run(funcData->chunk, newEnv);
                            stack = savedStack;
                            stack.push_back(result);
                        } else {
                            stack.clear();
                            Interpreter sub;
                            sub.currentEnv = newEnv;
                            Value result;
                            try {
                                if (funcData->exprBody) result = sub.evaluateExpr(funcData->exprBody);
                                else if (funcData->body) {
                                    for (const auto& s : funcData->body->stmts) result = sub.execute(s.get());
                                }
                            } catch (const Interpreter::ReturnSignal& ret) { result = ret.value; }
                            stack.push_back(result);
                        }
                    } else {
                        stack.resize(stack.size() - argCount - 1);
                        stack.push_back(Value::makeUndefined());
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
                    stack.push_back(Value::makeFunc(funcData));
                    break;
                }

                case OpCode::SCOPE_ENTER:
                    cf.env = cf.env->createChild();
                    break;

                case OpCode::SCOPE_EXIT:
                    if (cf.env->parent) cf.env = cf.env->parent;
                    break;

                case OpCode::HALT:
                    frames.clear();
                    break;

                default:
                    break;
            }
        }

        if (!stack.empty()) { Value r = stack.back(); stack.clear(); return r; }
        return Value::makeUndefined();
    }

    Value callValue(const Value& fn, const std::vector<Value>& args) {
        if (fn.type == ValueType::NativeFunction) return fn.nativeVal(args);
        if (fn.type == ValueType::Function) {
            auto funcData = fn.funcVal;
            auto newEnv = funcData->closure->createChild();
            for (size_t i = 0; i < funcData->params.size(); i++)
                newEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());

            if (funcData->chunk) {
                auto savedStack = stack;
                stack.clear();
                Value result = run(funcData->chunk, newEnv);
                stack = savedStack;
                return result;
            }

            stack.clear();
            Interpreter sub;
            sub.currentEnv = newEnv;
            Value result;
            try {
                if (funcData->exprBody) result = sub.evaluateExpr(funcData->exprBody);
                else if (funcData->body) {
                    for (const auto& s : funcData->body->stmts) result = sub.execute(s.get());
                }
            } catch (const Interpreter::ReturnSignal& ret) { result = ret.value; }
            return result;
        }
        return Value::makeUndefined();
    }

    Value getProperty(const Value& obj, const std::string& name) {
        if (obj.type == ValueType::String) {
            if (name == "length") return Value::makeNum((double)obj.strVal->str.size());
            if (name == "charAt")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    int i = a.empty()?0:(int)a[0].toNumber();
                    if(i<0||i>=(int)str.size()) return Value::makeStr("");
                    return Value::makeStr(std::string(1,str[i]));
                });
            if (name == "indexOf")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    auto p = str.find(a.empty()?"":a[0].toString());
                    return Value::makeNum(p==std::string::npos?-1.0:(double)p);
                });
            if (name == "slice")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    int s = a.empty()?0:(int)a[0].toNumber();
                    int e = a.size()<2?(int)str.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)str.size()+s);
                    if(e<0)e=std::max(0,(int)str.size()+e);
                    if(s>=e||s>=(int)str.size())return Value::makeStr("");
                    return Value::makeStr(str.substr(s,e-s));
                });
            if (name == "toUpperCase")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::toupper); return Value::makeStr(r);});
            if (name == "toLowerCase")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::tolower); return Value::makeStr(r);});
            if (name == "trim")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string s=str; s.erase(0,s.find_first_not_of(" \t\n\r")); s.erase(s.find_last_not_of(" \t\n\r")+1); return Value::makeStr(s);});
            if (name == "startsWith")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    return Value::makeBool(str.find(a.empty()?"":a[0].toString())==0);});
            if (name == "endsWith")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    std::string s=a.empty()?"":a[0].toString(); return Value::makeBool(str.size()>=s.size()&&str.substr(str.size()-s.size())==s);});
            if (name == "includes")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    return Value::makeBool(str.find(a.empty()?"":a[0].toString())!=std::string::npos);});
            if (name == "repeat")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    int c=a.empty()?0:(int)a[0].toNumber(); if(c<=0)return Value::makeStr("");
                    std::string r; r.reserve(str.size()*c); for(int i=0;i<c;i++)r+=str; return Value::makeStr(r);});
            return Value::makeUndefined();
        }

        if (obj.type == ValueType::Array) {
            if (name == "length") return Value::makeNum((double)obj.arrVal->elements.size());
            if (name == "push")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    for(const auto& v:a)arrPtr->elements.push_back(v); return Value::makeNum((double)arrPtr->elements.size());});
            if (name == "pop")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&) mutable->Value{
                    if(arrPtr->elements.empty())return Value::makeUndefined(); Value v=arrPtr->elements.back(); arrPtr->elements.pop_back(); return v;});
            if (name == "shift")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&) mutable->Value{
                    if(arrPtr->elements.empty())return Value::makeUndefined(); Value v=arrPtr->elements.front(); arrPtr->elements.erase(arrPtr->elements.begin()); return v;});
            if (name == "unshift")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    arrPtr->elements.insert(arrPtr->elements.begin(),a.begin(),a.end()); return Value::makeNum((double)arrPtr->elements.size());});
            if (name == "indexOf")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty())return Value::makeNum(-1);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value eq=arrPtr->elements[i].eq(a[0],true);if(eq.boolVal)return Value::makeNum((double)i);}
                    return Value::makeNum(-1);});
            if (name == "includes")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty())return Value::makeBool(false);
                    for(const auto& e:arrPtr->elements){Value eq=e.eq(a[0],true);if(eq.boolVal)return Value::makeBool(true);}
                    return Value::makeBool(false);});
            if (name == "join")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    std::string sep=a.empty()?",":a[0].toString(); std::string r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){if(i>0)r+=sep;r+=arrPtr->elements[i].toString();}
                    return Value::makeStr(r);});
            if (name == "slice")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    int s=a.empty()?0:(int)a[0].toNumber(); int e=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)arrPtr->elements.size()+s); if(e<0)e=std::max(0,(int)arrPtr->elements.size()+e);
                    Value r=Value::makeArr(); for(int i=s;i<e&&i<(int)arrPtr->elements.size();i++)r.arrVal->elements.push_back(arrPtr->elements[i]); return r;});
            if (name == "splice")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    int s=a.empty()?0:(int)a[0].toNumber(); int dc=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)arrPtr->elements.size()+s); dc=std::min(dc,(int)arrPtr->elements.size()-s);
                    Value rem=Value::makeArr(); auto it=arrPtr->elements.begin()+s;
                    for(int i=0;i<dc;i++)rem.arrVal->elements.push_back(*(it+i));
                    arrPtr->elements.erase(it,it+dc);
                    for(size_t i=2;i<a.size();i++)arrPtr->elements.insert(arrPtr->elements.begin()+s+(i-2),a[i]);
                    return rem;});
            if (name == "forEach")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        callValue(a[0], ca);}
                    return Value::makeUndefined();});
            if (name == "map")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        r.arrVal->elements.push_back(callValue(a[0], ca));}
                    return r;});
            if (name == "filter")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        Value rv=callValue(a[0], ca); if(rv.isTruthy())r.arrVal->elements.push_back(arrPtr->elements[i]);}
                    return r;});
            if (name == "reduce")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    bool hasInit=a.size()>1; Value acc=hasInit?a[1]:Value::makeUndefined();
                    size_t si=hasInit?0:1; if(!hasInit&&!arrPtr->elements.empty())acc=arrPtr->elements[0];
                    for(size_t i=si;i<arrPtr->elements.size();i++)
                        acc=callValue(a[0], {acc,arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});
                    return acc;});
            if (name == "find")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return arrPtr->elements[i];}
                    return Value::makeUndefined();});
            if (name == "some")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return Value::makeBool(true);}
                    return Value::makeBool(false);});
            if (name == "every")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(!r.isTruthy())return Value::makeBool(false);}
                    return Value::makeBool(true);});
            if (name == "sort")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[](const Value& x,const Value& y){return x.toNumber()<y.toNumber();});
                    else std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[this,&a](const Value& x,const Value& y){return callValue(a[0],{x,y}).toNumber()<0;});
                    Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
            if (name == "reverse")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&)->Value{
                    std::reverse(arrPtr->elements.begin(),arrPtr->elements.end()); Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
            if (name == "concat")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements;
                    for(const auto& v:a){if(v.isArray())r.arrVal->elements.insert(r.arrVal->elements.end(),v.arrVal->elements.begin(),v.arrVal->elements.end());else r.arrVal->elements.push_back(v);}
                    return r;});
            return Value::makeUndefined();
        }

        if (obj.type == ValueType::Object) return obj.getProperty(name);
        return Value::makeUndefined();
    }

private:
    uint16_t readShort(CallFrame& frame) {
        uint16_t b1 = *frame.ip++;
        uint16_t b2 = *frame.ip++;
        return b1 | (b2 << 8);
    }
};

}
