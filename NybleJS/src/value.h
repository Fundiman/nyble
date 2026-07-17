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
    std::shared_ptr<std::unordered_map<std::string, Value>> nativeProps;

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
    static Value makeError(const std::string& name, const std::string& message);
    static Value makeTypeError(const std::string& message);
    static Value makeReferenceError(const std::string& message);
    static Value makeSyntaxError(const std::string& message);
    static Value makeRangeError(const std::string& message);

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
    Value bitNot() const;
    Value preInc();
    Value preDec();
    Value bitAnd(const Value& other) const;
    Value bitOr(const Value& other) const;
    Value bitXor(const Value& other) const;
    Value shl(const Value& other) const;
    Value shr(const Value& other) const;
    Value ushr(const Value& other) const;
};

// Exception type for propagating JS-level errors through C++ catch sites
struct NybleRuntimeError {
    Value error;
    NybleRuntimeError(const Value& e) : error(e) {}
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
inline GCObject* gErrorPrototype = nullptr;
inline GCObject* gTypeErrorPrototype = nullptr;
inline GCObject* gReferenceErrorPrototype = nullptr;
inline GCObject* gSyntaxErrorPrototype = nullptr;
inline GCObject* gRangeErrorPrototype = nullptr;
inline GCObject* gDatePrototype = nullptr;

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

}
