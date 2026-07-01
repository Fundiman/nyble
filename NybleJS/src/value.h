#pragma once
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <iostream>
#include <algorithm>
#include "gc.h"

namespace nyble {

struct Environment;
struct BlockStmt;
struct BytecodeChunk;
struct Expr;

struct GCObject;
struct GCArray;
struct GCFunction;

enum class ValueType {
    Null,
    Undefined,
    Boolean,
    Number,
    String,
    Object,
    Array,
    Function,
    NativeFunction
};

using NativeFn = std::function<class Value(const std::vector<class Value>&, const class Value&)>;

// Global call dispatcher for native functions that need to invoke other functions
inline std::function<class Value(const class Value&, const std::vector<class Value>&, const class Value&)> g_callFunction;

class Value {
public:
    ValueType type;

    bool boolVal;
    double numVal;
    GCString* strVal;
    GCObject* objVal;
    GCArray* arrVal;
    GCFunction* funcVal;
    NativeFn nativeVal;

    Value() : type(ValueType::Undefined), boolVal(false), numVal(0), strVal(nullptr), objVal(nullptr), arrVal(nullptr), funcVal(nullptr) {}

    static Value makeNull() { Value v; v.type = ValueType::Null; return v; }
    static Value makeUndefined() { Value v; v.type = ValueType::Undefined; return v; }
    static Value makeBool(bool b) { Value v; v.type = ValueType::Boolean; v.boolVal = b; return v; }
    static Value makeNum(double n) { Value v; v.type = ValueType::Number; v.numVal = n; return v; }
    static Value makeStr(const std::string& s);
    static Value makeObj();
    static Value makeArr();
    static Value makeFunc(GCFunction* f) { Value v; v.type = ValueType::Function; v.funcVal = f; return v; }
    static Value makeNative(NativeFn f) { Value v; v.type = ValueType::NativeFunction; v.nativeVal = std::move(f); return v; }

    bool isTruthy() const {
        switch (type) {
            case ValueType::Null: return false;
            case ValueType::Undefined: return false;
            case ValueType::Boolean: return boolVal;
            case ValueType::Number: return numVal != 0 && !std::isnan(numVal);
            case ValueType::String: return strVal && !strVal->str.empty();
            default: return true;
        }
    }

    std::string toString() const;
    double toNumber() const;

    bool toBool() const { return isTruthy(); }

    bool isNumber() const { return type == ValueType::Number; }
    bool isString() const { return type == ValueType::String; }
    bool isObject() const { return type == ValueType::Object; }
    bool isArray() const { return type == ValueType::Array; }
    bool isFunction() const { return type == ValueType::Function || type == ValueType::NativeFunction; }
    bool isNull() const { return type == ValueType::Null; }
    bool isUndefined() const { return type == ValueType::Undefined; }

    Value getProperty(const std::string& name) const;
    void setProperty(const std::string& name, const Value& val);
    Value getIndex(size_t i) const;
    void setIndex(size_t i, const Value& val);

    Value add(const Value& other) const;
    Value sub(const Value& other) const;
    Value mul(const Value& other) const;
    Value div(const Value& other) const;
    Value mod(const Value& other) const;
    Value poww(const Value& other) const;
    Value eq(const Value& other, bool strict) const;
    Value cmp(const Value& other, const std::string& op) const;
    Value negate() const;
    Value logicalNot() const;
    Value typeOf() const;
    Value unaryMinus() const;
    Value unaryPlus() const;
    Value preInc();
    Value preDec();
};

struct GCObject : GCHeader {
    std::unordered_map<std::string, Value> properties;
    GCObject* proto = nullptr;
    GCObject() : GCHeader(GCType::Object) {}
    void trace(std::vector<GCHeader*>& worklist) override;
    size_t approxSize() const override { return sizeof(GCObject) + properties.size() * 64; }
};
inline GCObject* gObjectPrototype = nullptr;
inline GCObject* gFunctionPrototype = nullptr;

struct GCArray : GCHeader {
    std::vector<Value> elements;
    std::unordered_map<std::string, Value> properties;
    GCObject* proto = nullptr;
    GCArray() : GCHeader(GCType::Array) {}
    void trace(std::vector<GCHeader*>& worklist) override;
    size_t approxSize() const override { return sizeof(GCArray) + elements.capacity() * sizeof(Value) + properties.size() * 64; }
};

struct GCFunction : GCHeader {
    std::vector<std::string> params;
    std::shared_ptr<Environment> closure;
    const BlockStmt* body;
    Expr* exprBody;
    BytecodeChunk* chunk;
    std::unordered_map<std::string, Value> properties;
    GCObject* proto = nullptr;
    bool isArrow = false;
    GCFunction() : GCHeader(GCType::Function), body(nullptr), exprBody(nullptr), chunk(nullptr), isArrow(false) {}
    void trace(std::vector<GCHeader*>& worklist) override;
    size_t approxSize() const override { return sizeof(GCFunction) + params.size() * 32; }
};

inline Value Value::makeStr(const std::string& s) {
    gHeap.checkBudget(sizeof(GCString) + s.capacity());
    Value v;
    v.type = ValueType::String;
    v.strVal = gHeap.allocate<GCString>(s);
    return v;
}

inline Value Value::makeObj() {
    Value v;
    v.type = ValueType::Object;
    v.objVal = gHeap.allocate<GCObject>();
    return v;
}

inline Value Value::makeArr() {
    Value v;
    v.type = ValueType::Array;
    v.arrVal = gHeap.allocate<GCArray>();
    return v;
}

inline std::string Value::toString() const {
    switch (type) {
        case ValueType::Null: return "null";
        case ValueType::Undefined: return "undefined";
        case ValueType::Boolean: return boolVal ? "true" : "false";
        case ValueType::Number: {
            if (std::isnan(numVal)) return "NaN";
            if (std::isinf(numVal)) return numVal > 0 ? "Infinity" : "-Infinity";
            std::ostringstream os;
            os.precision(17);
            os << numVal;
            std::string s = os.str();
            if (s.find('.') != std::string::npos) {
                s.erase(s.find_last_not_of('0') + 1, std::string::npos);
                if (s.back() == '.') s.pop_back();
            }
            return s;
        }
        case ValueType::String: return strVal ? strVal->str : "";
        case ValueType::Object: return "[object Object]";
        case ValueType::Array: {
            if (!arrVal) return "[]";
            std::string r = "[";
            for (size_t i = 0; i < arrVal->elements.size(); i++) {
                if (i > 0) r += ", ";
                r += arrVal->elements[i].toString();
            }
            return r + "]";
        }
        case ValueType::Function: return "[function]";
        case ValueType::NativeFunction: return "[native function]";
    }
    return "unknown";
}

inline double Value::toNumber() const {
    switch (type) {
        case ValueType::Null: return 0;
        case ValueType::Undefined: return std::numeric_limits<double>::quiet_NaN();
        case ValueType::Boolean: return boolVal ? 1 : 0;
        case ValueType::Number: return numVal;
        case ValueType::String: {
            if (!strVal) return 0;
            std::string s = strVal->str;
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
            if (s.empty()) return 0;
            char* end = nullptr;
            double r = std::strtod(s.c_str(), &end);
            if (end != s.c_str() + s.length()) return std::numeric_limits<double>::quiet_NaN();
            return r;
        }
        default: return std::numeric_limits<double>::quiet_NaN();
    }
}

inline Value Value::getProperty(const std::string& name) const {
    if (type == ValueType::Object && objVal) {
        auto it = objVal->properties.find(name);
        if (it != objVal->properties.end()) return it->second;
        GCObject* p = objVal->proto;
        while (p) {
            auto it2 = p->properties.find(name);
            if (it2 != p->properties.end()) return it2->second;
            p = p->proto;
        }
    }
    if (type == ValueType::Array && arrVal) {
        if (name == "length") return Value::makeNum((double)arrVal->elements.size());
        auto it = arrVal->properties.find(name);
        if (it != arrVal->properties.end()) return it->second;
        GCObject* p = arrVal->proto;
        while (p) {
            auto it2 = p->properties.find(name);
            if (it2 != p->properties.end()) return it2->second;
            p = p->proto;
        }
    }
    if (type == ValueType::Function && funcVal) {
        auto it = funcVal->properties.find(name);
        if (it != funcVal->properties.end()) return it->second;
        GCObject* p = funcVal->proto;
        while (p) {
            auto it2 = p->properties.find(name);
            if (it2 != p->properties.end()) return it2->second;
            p = p->proto;
        }
    }
    return Value::makeUndefined();
}

inline void Value::setProperty(const std::string& name, const Value& val) {
    if (type == ValueType::Object && objVal) {
        objVal->properties[name] = val;
    } else if (type == ValueType::Array && arrVal) {
        arrVal->properties[name] = val;
    } else if (type == ValueType::Function && funcVal) {
        funcVal->properties[name] = val;
    }
}

inline Value Value::getIndex(size_t i) const {
    if (type == ValueType::Array && arrVal && i < arrVal->elements.size())
        return arrVal->elements[i];
    if (type == ValueType::String && strVal && i < strVal->str.size())
        return Value::makeStr(std::string(1, strVal->str[i]));
    return Value::makeUndefined();
}

inline void Value::setIndex(size_t i, const Value& val) {
    if (type == ValueType::Array && arrVal) {
        if (i >= arrVal->elements.size()) arrVal->elements.resize(i + 1);
        arrVal->elements[i] = val;
    }
}

inline Value Value::add(const Value& other) const {
    if ((type == ValueType::String || other.type == ValueType::String) &&
        (isString() || other.isString() || isNumber() || other.isNumber())) {
        return Value::makeStr(toString() + other.toString());
    }
    return Value::makeNum(toNumber() + other.toNumber());
}

inline Value Value::sub(const Value& other) const {
    return Value::makeNum(toNumber() - other.toNumber());
}

inline Value Value::mul(const Value& other) const {
    return Value::makeNum(toNumber() * other.toNumber());
}

inline Value Value::div(const Value& other) const {
    return Value::makeNum(toNumber() / other.toNumber());
}

inline Value Value::mod(const Value& other) const {
    return Value::makeNum(std::fmod(toNumber(), other.toNumber()));
}

inline Value Value::poww(const Value& other) const {
    return Value::makeNum(std::pow(toNumber(), other.toNumber()));
}

inline Value Value::eq(const Value& other, bool strict) const {
    if (strict) {
        if (type != other.type) return Value::makeBool(false);
        switch (type) {
            case ValueType::Null: return Value::makeBool(true);
            case ValueType::Undefined: return Value::makeBool(true);
            case ValueType::Boolean: return Value::makeBool(boolVal == other.boolVal);
            case ValueType::Number: return Value::makeBool(numVal == other.numVal);
            case ValueType::String: return Value::makeBool(strVal && other.strVal && strVal->str == other.strVal->str);
            case ValueType::Object: return Value::makeBool(objVal == other.objVal);
            case ValueType::Array: return Value::makeBool(arrVal == other.arrVal);
            case ValueType::Function: return Value::makeBool(funcVal == other.funcVal);
            default: return Value::makeBool(false);
        }
    }
    if (type == other.type) return eq(other, true);
    if (type == ValueType::Null && other.type == ValueType::Undefined) return Value::makeBool(true);
    if (type == ValueType::Undefined && other.type == ValueType::Null) return Value::makeBool(true);
    if (type == ValueType::Number || other.type == ValueType::Number) {
        return Value::makeBool(toNumber() == other.toNumber());
    }
    return Value::makeBool(false);
}

inline Value Value::cmp(const Value& other, const std::string& op) const {
    double a = toNumber(), b = other.toNumber();
    if (op == "<") return Value::makeBool(a < b);
    if (op == ">") return Value::makeBool(a > b);
    if (op == "<=") return Value::makeBool(a <= b);
    if (op == ">=") return Value::makeBool(a >= b);
    return Value::makeBool(false);
}

inline Value Value::negate() const {
    return Value::makeNum(-toNumber());
}

inline Value Value::logicalNot() const {
    return Value::makeBool(!isTruthy());
}

inline Value Value::typeOf() const {
    switch (type) {
        case ValueType::Null: return Value::makeStr("object");
        case ValueType::Undefined: return Value::makeStr("undefined");
        case ValueType::Boolean: return Value::makeStr("boolean");
        case ValueType::Number: return Value::makeStr("number");
        case ValueType::String: return Value::makeStr("string");
        case ValueType::Object: return Value::makeStr("object");
        case ValueType::Array: return Value::makeStr("object");
        case ValueType::Function: return Value::makeStr("function");
        case ValueType::NativeFunction: return Value::makeStr("function");
    }
    return Value::makeStr("unknown");
}

inline Value Value::unaryMinus() const { return Value::makeNum(-toNumber()); }
inline Value Value::unaryPlus() const { return Value::makeNum(toNumber()); }
inline Value Value::preInc() {
    if (type == ValueType::Number) numVal += 1;
    else { type = ValueType::Number; numVal = 1; }
    return *this;
}
inline Value Value::preDec() {
    if (type == ValueType::Number) numVal -= 1;
    else { type = ValueType::Number; numVal = -1; }
    return *this;
}

inline void GCObject::trace(std::vector<GCHeader*>& worklist) {
    for (auto& [key, val] : properties) {
        GCHeader* h = nullptr;
        switch (val.type) {
            case ValueType::String: h = val.strVal; break;
            case ValueType::Object: h = val.objVal; break;
            case ValueType::Array:  h = val.arrVal; break;
            case ValueType::Function: h = val.funcVal; break;
            default: continue;
        }
        if (h && !h->marked) {
            h->marked = true;
            worklist.push_back(h);
        }
    }
    if (proto && !proto->marked) {
        proto->marked = true;
        worklist.push_back(proto);
    }
}

inline void GCArray::trace(std::vector<GCHeader*>& worklist) {
    for (auto& val : elements) {
        GCHeader* h = nullptr;
        switch (val.type) {
            case ValueType::String: h = val.strVal; break;
            case ValueType::Object: h = val.objVal; break;
            case ValueType::Array:  h = val.arrVal; break;
            case ValueType::Function: h = val.funcVal; break;
            default: continue;
        }
        if (h && !h->marked) {
            h->marked = true;
            worklist.push_back(h);
        }
    }
    for (auto& [key, val] : properties) {
        GCHeader* h = nullptr;
        switch (val.type) {
            case ValueType::String: h = val.strVal; break;
            case ValueType::Object: h = val.objVal; break;
            case ValueType::Array:  h = val.arrVal; break;
            case ValueType::Function: h = val.funcVal; break;
            default: continue;
        }
        if (h && !h->marked) {
            h->marked = true;
            worklist.push_back(h);
        }
    }
    if (proto && !proto->marked) {
        proto->marked = true;
        worklist.push_back(proto);
    }
}

inline void GCHeap::collect() {
    if (!head) return;

    {
        GCHeader* p = head;
        int limit = allocCount + 1;
        while (p && limit-- > 0) {
            p->marked = false;
            p = p->next;
        }
    }

    std::vector<GCHeader*> worklist;
    if (rootTracer) rootTracer(worklist);

    for (GCHeader* h : tempRoots) {
        if (h && !h->marked) {
            h->marked = true;
            worklist.push_back(h);
        }
    }

    while (!worklist.empty()) {
        GCHeader* obj = worklist.back();
        worklist.pop_back();
        obj->trace(worklist);
    }

    sweep();
}

inline void GCHeap::sweep() {
    GCHeader** pp = &head;
    int limit = allocCount + 1;
    while (*pp && limit-- > 0) {
        if (!(*pp)->marked) {
            GCHeader* dead = *pp;
            if (totalBytes >= dead->approxSize())
                totalBytes -= dead->approxSize();
            else
                totalBytes = 0;
            *pp = dead->next;
            allocCount--;
            delete dead;
        } else {
            pp = &(*pp)->next;
        }
    }
    nextGC = allocCount * 2 + 256;
}

inline GCHeap::~GCHeap() {
    GCHeader* p = head;
    int limit = allocCount + 1;
    while (p && limit-- > 0) {
        GCHeader* next = p->next;
        delete p;
        p = next;
    }
    head = nullptr;
}

}
