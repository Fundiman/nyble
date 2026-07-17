#include "value.h"

namespace nyble {

Value Value::makeStr(const std::string& s) {
    gHeap.checkBudget(sizeof(GCString) + s.capacity());
    Value v;
    v.type = ValueType::String;
    v.strVal = gHeap.allocate<GCString>(s);
    return v;
}

Value Value::makeObj() {
    Value v;
    v.type = ValueType::Object;
    v.objVal = gHeap.allocate<GCObject>();
    return v;
}

Value Value::makeArr() {
    Value v;
    v.type = ValueType::Array;
    v.arrVal = gHeap.allocate<GCArray>();
    return v;
}

Value Value::makeError(const std::string& name, const std::string& message) {
    Value v = makeObj();
    v.objVal->properties["name"] = makeStr(name);
    v.objVal->properties["message"] = makeStr(message);
    v.objVal->properties["stack"] = makeStr(name + ": " + message);
    return v;
}

Value Value::makeTypeError(const std::string& message) {
    Value v = makeError("TypeError", message);
    if (gTypeErrorPrototype) v.objVal->proto = gTypeErrorPrototype;
    else if (gErrorPrototype) v.objVal->proto = gErrorPrototype;
    return v;
}

Value Value::makeReferenceError(const std::string& message) {
    Value v = makeError("ReferenceError", message);
    if (gReferenceErrorPrototype) v.objVal->proto = gReferenceErrorPrototype;
    else if (gErrorPrototype) v.objVal->proto = gErrorPrototype;
    return v;
}

Value Value::makeSyntaxError(const std::string& message) {
    Value v = makeError("SyntaxError", message);
    if (gSyntaxErrorPrototype) v.objVal->proto = gSyntaxErrorPrototype;
    else if (gErrorPrototype) v.objVal->proto = gErrorPrototype;
    return v;
}

Value Value::makeRangeError(const std::string& message) {
    Value v = makeError("RangeError", message);
    if (gRangeErrorPrototype) v.objVal->proto = gRangeErrorPrototype;
    else if (gErrorPrototype) v.objVal->proto = gErrorPrototype;
    return v;
}

std::string Value::toString() const {
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
        case ValueType::Object: {
            if (!objVal) return "[object Object]";
            auto dateIt = objVal->properties.find("__dateValue__");
            if (dateIt != objVal->properties.end() && gDatePrototype) {
                GCObject* p = objVal->proto;
                while (p) {
                    auto fnIt = p->properties.find("toString");
                    if (fnIt != p->properties.end()) {
                        Value result = fnIt->second.nativeVal({}, *this);
                        return result.toString();
                    }
                    p = p->proto;
                }
            }
            auto nameIt = objVal->properties.find("name");
            auto msgIt = objVal->properties.find("message");
            if (nameIt != objVal->properties.end()) {
                std::string name = nameIt->second.toString();
                if (msgIt != objVal->properties.end()) {
                    std::string msg = msgIt->second.toString();
                    if (msg.empty()) return name;
                    return name + ": " + msg;
                }
                return name;
            }
            return "[object Object]";
        }
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

double Value::toNumber() const {
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

Value Value::getProperty(const std::string& name) const {
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
    if (type == ValueType::NativeFunction && nativeProps) {
        auto it = nativeProps->find(name);
        if (it != nativeProps->end()) return it->second;
    }
    return Value::makeUndefined();
}

void Value::setProperty(const std::string& name, const Value& val) {
    if (type == ValueType::Object && objVal) {
        objVal->properties[name] = val;
    } else if (type == ValueType::Array && arrVal) {
        arrVal->properties[name] = val;
    } else if (type == ValueType::Function && funcVal) {
        funcVal->properties[name] = val;
    } else if (type == ValueType::NativeFunction) {
        if (!nativeProps) nativeProps = std::make_shared<std::unordered_map<std::string, Value>>();
        (*nativeProps)[name] = val;
    }
}

Value Value::getIndex(size_t i) const {
    if (type == ValueType::Array && arrVal && i < arrVal->elements.size())
        return arrVal->elements[i];
    if (type == ValueType::String && strVal && i < strVal->str.size())
        return Value::makeStr(std::string(1, strVal->str[i]));
    return Value::makeUndefined();
}

void Value::setIndex(size_t i, const Value& val) {
    if (type == ValueType::Array && arrVal) {
        if (i >= arrVal->elements.size()) arrVal->elements.resize(i + 1);
        arrVal->elements[i] = val;
    }
}

Value Value::add(const Value& other) const {
    if ((type == ValueType::String || other.type == ValueType::String) &&
        (isString() || other.isString() || isNumber() || other.isNumber())) {
        return Value::makeStr(toString() + other.toString());
    }
    return Value::makeNum(toNumber() + other.toNumber());
}

Value Value::sub(const Value& other) const {
    return Value::makeNum(toNumber() - other.toNumber());
}

Value Value::mul(const Value& other) const {
    return Value::makeNum(toNumber() * other.toNumber());
}

Value Value::div(const Value& other) const {
    return Value::makeNum(toNumber() / other.toNumber());
}

Value Value::mod(const Value& other) const {
    return Value::makeNum(std::fmod(toNumber(), other.toNumber()));
}

Value Value::poww(const Value& other) const {
    return Value::makeNum(std::pow(toNumber(), other.toNumber()));
}

Value Value::eq(const Value& other, bool strict) const {
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

Value Value::cmp(const Value& other, const std::string& op) const {
    double a = toNumber(), b = other.toNumber();
    if (op == "<") return Value::makeBool(a < b);
    if (op == ">") return Value::makeBool(a > b);
    if (op == "<=") return Value::makeBool(a <= b);
    if (op == ">=") return Value::makeBool(a >= b);
    return Value::makeBool(false);
}

Value Value::negate() const {
    return Value::makeNum(-toNumber());
}

Value Value::logicalNot() const {
    return Value::makeBool(!isTruthy());
}

Value Value::typeOf() const {
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

Value Value::unaryMinus() const { return Value::makeNum(-toNumber()); }
Value Value::unaryPlus() const { return Value::makeNum(toNumber()); }

Value Value::preInc() {
    if (type == ValueType::Number) numVal += 1;
    else { type = ValueType::Number; numVal = 1; }
    return *this;
}

Value Value::preDec() {
    if (type == ValueType::Number) numVal -= 1;
    else { type = ValueType::Number; numVal = -1; }
    return *this;
}

static int32_t toInt32(double v) {
    if (std::isnan(v) || std::isinf(v) || v == 0.0) return 0;
    double two32 = 4294967296.0;
    double d = std::fmod(v, two32);
    if (d < 0) d += two32;
    if (d >= 2147483648.0) d -= two32;
    return (int32_t)d;
}

static uint32_t toUint32(double v) {
    return (uint32_t)toInt32(v);
}

Value Value::bitNot() const {
    return Value::makeNum(~toInt32(toNumber()));
}

Value Value::bitAnd(const Value& other) const {
    return Value::makeNum(toInt32(toNumber()) & toInt32(other.toNumber()));
}

Value Value::bitOr(const Value& other) const {
    return Value::makeNum(toInt32(toNumber()) | toInt32(other.toNumber()));
}

Value Value::bitXor(const Value& other) const {
    return Value::makeNum(toInt32(toNumber()) ^ toInt32(other.toNumber()));
}

Value Value::shl(const Value& other) const {
    int32_t l = toInt32(toNumber());
    int32_t r = toUint32(other.toNumber()) & 0x1F;
    return Value::makeNum(l << r);
}

Value Value::shr(const Value& other) const {
    int32_t l = toInt32(toNumber());
    int32_t r = toUint32(other.toNumber()) & 0x1F;
    return Value::makeNum(l >> r);
}

Value Value::ushr(const Value& other) const {
    uint32_t l = toUint32(toNumber());
    uint32_t r = toUint32(other.toNumber()) & 0x1F;
    return Value::makeNum((double)((uint32_t)(l >> r)));
}

void GCObject::trace(std::vector<GCHeader*>& worklist) {
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

void GCArray::trace(std::vector<GCHeader*>& worklist) {
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

}
