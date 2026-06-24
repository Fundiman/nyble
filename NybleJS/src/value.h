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

namespace nyble {

struct Environment;
struct BlockStmt;
struct BytecodeChunk;

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

struct ObjectData {
    std::unordered_map<std::string, class Value> properties;
};

struct ArrayData {
    std::vector<class Value> elements;
};

struct BytecodeChunk;

struct FunctionData {
    std::vector<std::string> params;
    const BlockStmt* body;
    class Expr* exprBody;
    std::shared_ptr<Environment> closure;
    BytecodeChunk* chunk;
    FunctionData() : body(nullptr), exprBody(nullptr), chunk(nullptr) {}
};

using NativeFn = std::function<class Value(const std::vector<class Value>&)>;

class Value {
public:
    ValueType type;

    bool boolVal;
    double numVal;
    std::string strVal;
    ObjectData objVal;
    ArrayData arrVal;
    std::shared_ptr<FunctionData> funcVal;
    NativeFn nativeVal;

    Value() : type(ValueType::Undefined), boolVal(false), numVal(0) {}

    static Value makeNull() { Value v; v.type = ValueType::Null; return v; }
    static Value makeUndefined() { Value v; v.type = ValueType::Undefined; return v; }
    static Value makeBool(bool b) { Value v; v.type = ValueType::Boolean; v.boolVal = b; return v; }
    static Value makeNum(double n) { Value v; v.type = ValueType::Number; v.numVal = n; return v; }
    static Value makeStr(const std::string& s) { Value v; v.type = ValueType::String; v.strVal = s; return v; }
    static Value makeObj() { Value v; v.type = ValueType::Object; return v; }
    static Value makeArr() { Value v; v.type = ValueType::Array; return v; }
    static Value makeFunc(std::shared_ptr<FunctionData> f) { Value v; v.type = ValueType::Function; v.funcVal = std::move(f); return v; }
    static Value makeNative(NativeFn f) { Value v; v.type = ValueType::NativeFunction; v.nativeVal = std::move(f); return v; }

    bool isTruthy() const {
        switch (type) {
            case ValueType::Null: return false;
            case ValueType::Undefined: return false;
            case ValueType::Boolean: return boolVal;
            case ValueType::Number: return numVal != 0 && !std::isnan(numVal);
            case ValueType::String: return !strVal.empty();
            default: return true;
        }
    }

    std::string toString() const {
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
            case ValueType::String: return strVal;
            case ValueType::Object: return "[object Object]";
            case ValueType::Array: {
                std::string r = "[";
                for (size_t i = 0; i < arrVal.elements.size(); i++) {
                    if (i > 0) r += ", ";
                    r += arrVal.elements[i].toString();
                }
                return r + "]";
            }
            case ValueType::Function: return "[function]";
            case ValueType::NativeFunction: return "[native function]";
        }
        return "unknown";
    }

    double toNumber() const {
        switch (type) {
            case ValueType::Null: return 0;
            case ValueType::Undefined: return std::numeric_limits<double>::quiet_NaN();
            case ValueType::Boolean: return boolVal ? 1 : 0;
            case ValueType::Number: return numVal;
            case ValueType::String: {
                std::string s = strVal;
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

    bool toBool() const {
        return isTruthy();
    }

    bool isNumber() const { return type == ValueType::Number; }
    bool isString() const { return type == ValueType::String; }
    bool isObject() const { return type == ValueType::Object; }
    bool isArray() const { return type == ValueType::Array; }
    bool isFunction() const { return type == ValueType::Function || type == ValueType::NativeFunction; }
    bool isNull() const { return type == ValueType::Null; }
    bool isUndefined() const { return type == ValueType::Undefined; }

    Value getProperty(const std::string& name) const {
        if (type == ValueType::Object) {
            auto it = objVal.properties.find(name);
            if (it != objVal.properties.end()) return it->second;
        }
        if (type == ValueType::Array) {
            if (name == "length") return Value::makeNum((double)arrVal.elements.size());
            auto it = objVal.properties.find(name);
            if (it != objVal.properties.end()) return it->second;
        }
        return Value::makeUndefined();
    }

    void setProperty(const std::string& name, const Value& val) {
        objVal.properties[name] = val;
    }

    Value getIndex(size_t i) const {
        if (type == ValueType::Array && i < arrVal.elements.size())
            return arrVal.elements[i];
        if (type == ValueType::String && i < strVal.size())
            return Value::makeStr(std::string(1, strVal[i]));
        return Value::makeUndefined();
    }

    void setIndex(size_t i, const Value& val) {
        if (type == ValueType::Array) {
            if (i >= arrVal.elements.size()) arrVal.elements.resize(i + 1);
            arrVal.elements[i] = val;
        }
    }

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

inline Value Value::add(const Value& other) const {
    if (type == ValueType::String || other.type == ValueType::String) {
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
    double r = other.toNumber();
    if (r == 0) {
        double n = toNumber();
        if (n == 0) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        if (n > 0) return Value::makeNum(INFINITY);
        return Value::makeNum(-INFINITY);
    }
    return Value::makeNum(toNumber() / r);
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
            case ValueType::String: return Value::makeBool(strVal == other.strVal);
            default: return Value::makeBool(this == &other);
        }
    }
    if (type == ValueType::Null && other.type == ValueType::Null) return Value::makeBool(true);
    if (type == ValueType::Undefined && other.type == ValueType::Undefined) return Value::makeBool(true);
    if (type == ValueType::Null || other.type == ValueType::Null) return Value::makeBool(false);
    if (type == ValueType::Undefined || other.type == ValueType::Undefined) return Value::makeBool(false);
    if (type == ValueType::Number && other.type == ValueType::Number) return Value::makeBool(numVal == other.numVal);
    if (type == ValueType::String && other.type == ValueType::String) return Value::makeBool(strVal == other.strVal);
    if (type == ValueType::Boolean) return Value::makeBool(toNumber() == other.toNumber());
    if (other.type == ValueType::Boolean) return Value::makeBool(toNumber() == other.toNumber());
    if (type == ValueType::String) return Value::makeBool(toNumber() == other.toNumber());
    if (other.type == ValueType::String) return Value::makeBool(toNumber() == other.toNumber());
    return Value::makeBool(false);
}

inline Value Value::cmp(const Value& other, const std::string& op) const {
    double a = toNumber();
    double b = other.toNumber();
    if (op == "<") return Value::makeBool(a < b);
    if (op == ">") return Value::makeBool(a > b);
    if (op == "<=") return Value::makeBool(a <= b);
    if (op == ">=") return Value::makeBool(a >= b);
    return Value::makeUndefined();
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

}
