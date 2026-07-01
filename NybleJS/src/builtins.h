#pragma once
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cfloat>
#include "env.h"

namespace nyble {

inline void installBuiltins(std::shared_ptr<Environment> env) {
    // console
    Value consoleObj = Value::makeObj();

    consoleObj.setProperty("log", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << std::endl;
        return Value::makeUndefined();
    }));

    consoleObj.setProperty("error", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cerr << " ";
            std::cerr << args[i].toString();
        }
        std::cerr << "\n";
        return Value::makeUndefined();
    }));

    consoleObj.setProperty("warn", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << "\n";
        return Value::makeUndefined();
    }));

    consoleObj.setProperty("time", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        static std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers;
        std::string label = args.empty() ? "default" : args[0].toString();
        timers[label] = std::chrono::steady_clock::now();
        return Value::makeUndefined();
    }));

    consoleObj.setProperty("timeEnd", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        static std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers;
        std::string label = args.empty() ? "default" : args[0].toString();
        auto it = timers.find(label);
        if (it != timers.end()) {
            auto end = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration<double, std::milli>(end - it->second).count();
            std::cout << label << ": " << ms << " ms\n";
            timers.erase(it);
        }
        return Value::makeUndefined();
    }));

    consoleObj.setProperty("assert", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty() || !args[0].isTruthy()) {
            std::string msg = args.size() > 1 ? args[1].toString() : "console.assert failed";
            std::cerr << "Assertion failed: " << msg << "\n";
        }
        return Value::makeUndefined();
    }));

    env->define("console", consoleObj);

    // Math
    Value mathObj = Value::makeObj();
    mathObj.setProperty("PI", Value::makeNum(3.14159265358979323846));
    mathObj.setProperty("E", Value::makeNum(2.71828182845904523536));
    mathObj.setProperty("LN2", Value::makeNum(0.69314718055994530942));
    mathObj.setProperty("LN10", Value::makeNum(2.30258509299404568402));
    mathObj.setProperty("LOG2E", Value::makeNum(1.44269504088896340736));
    mathObj.setProperty("LOG10E", Value::makeNum(0.43429448190325182765));
    mathObj.setProperty("SQRT2", Value::makeNum(1.41421356237309504880));
    mathObj.setProperty("SQRT1_2", Value::makeNum(0.70710678118654752440));

    mathObj.setProperty("abs", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::abs(v));
    }));

    mathObj.setProperty("floor", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::floor(v));
    }));

    mathObj.setProperty("ceil", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::ceil(v));
    }));

    mathObj.setProperty("round", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::round(v));
    }));

    mathObj.setProperty("trunc", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::trunc(v));
    }));

    mathObj.setProperty("sqrt", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::sqrt(v));
    }));

    mathObj.setProperty("cbrt", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::cbrt(v));
    }));

    mathObj.setProperty("pow", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double base = args.empty() ? 0 : args[0].toNumber();
        double exp = args.size() < 2 ? 0 : args[1].toNumber();
        return Value::makeNum(std::pow(base, exp));
    }));

    mathObj.setProperty("exp", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::exp(v));
    }));

    mathObj.setProperty("log", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::log(v));
    }));

    mathObj.setProperty("log2", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::log2(v));
    }));

    mathObj.setProperty("log10", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        return Value::makeNum(std::log10(v));
    }));

    mathObj.setProperty("min", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeNum(INFINITY);
        double m = args[0].toNumber();
        for (size_t i = 1; i < args.size(); i++) m = std::min(m, args[i].toNumber());
        return Value::makeNum(m);
    }));

    mathObj.setProperty("max", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeNum(-INFINITY);
        double m = args[0].toNumber();
        for (size_t i = 1; i < args.size(); i++) m = std::max(m, args[i].toNumber());
        return Value::makeNum(m);
    }));

    mathObj.setProperty("random", Value::makeNative([](const std::vector<Value>&, const Value&) -> Value {
        static std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value::makeNum(dist(gen));
    }));

    mathObj.setProperty("sin", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::sin(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("cos", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::cos(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("tan", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::tan(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("asin", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::asin(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("acos", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::acos(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("atan", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(std::atan(args.empty() ? 0 : args[0].toNumber()));
    }));

    mathObj.setProperty("atan2", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double y = args.empty() ? 0 : args[0].toNumber();
        double x = args.size() < 2 ? 0 : args[1].toNumber();
        return Value::makeNum(std::atan2(y, x));
    }));

    mathObj.setProperty("sign", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double v = args.empty() ? 0 : args[0].toNumber();
        if (v > 0) return Value::makeNum(1);
        if (v < 0) return Value::makeNum(-1);
        return Value::makeNum(0);
    }));

    mathObj.setProperty("hypot", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        double sum = 0;
        for (const auto& a : args) sum += a.toNumber() * a.toNumber();
        return Value::makeNum(std::sqrt(sum));
    }));

    mathObj.setProperty("clz32", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        unsigned int v = (unsigned int)(args.empty() ? 0 : (int)args[0].toNumber());
        return Value::makeNum((double)__builtin_clz(v));
    }));

    mathObj.setProperty("imul", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        int a = (int)(args.empty() ? 0 : args[0].toNumber());
        int b = (int)(args.size() < 2 ? 0 : args[1].toNumber());
        return Value::makeNum((double)(a * b));
    }));

    mathObj.setProperty("fround", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        float v = (float)(args.empty() ? 0 : args[0].toNumber());
        return Value::makeNum((double)v);
    }));

    env->define("Math", mathObj);

    // JSON
    Value jsonObj = Value::makeObj();
    jsonObj.setProperty("parse", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeUndefined();
        // Simple JSON string parsing (handles basic cases)
        std::string s = args[0].toString();
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
        if (s.empty()) return Value::makeUndefined();
        if (s[0] == '"') {
            return Value::makeStr(s.substr(1, s.size() - 2));
        }
        if (s == "true") return Value::makeBool(true);
        if (s == "false") return Value::makeBool(false);
        if (s == "null") return Value::makeNull();
        if (s[0] == '{') {
            Value obj = Value::makeObj();
            // Basic object parsing
            size_t i = 1;
            while (i < s.size() && s[i] != '}') {
                while (i < s.size() && (s[i] == ' ' || s[i] == '"')) i++;
                if (i >= s.size() || s[i] == '}') break;
                size_t start = i;
                while (i < s.size() && s[i] != '"') i++;
                std::string key = s.substr(start, i - start);
                if (i < s.size()) i++;
                while (i < s.size() && s[i] != ':') i++;
                if (i < s.size()) i++;
                size_t valStart = i;
                int depth = 0;
                while (i < s.size()) {
                    if (s[i] == '{' || s[i] == '[') depth++;
                    else if (s[i] == '}' || s[i] == ']') { depth--; if (depth < 0) break; }
                    else if (s[i] == ',' && depth == 0) break;
                    i++;
                }
                obj.setProperty(key, Value::makeStr(s.substr(valStart, i - valStart)));
            }
            return obj;
        }
        if (s[0] == '[') {
            Value arr = Value::makeArr();
            size_t i = 1;
            while (i < s.size() && s[i] != ']') {
                while (i < s.size() && (s[i] == ' ' || s[i] == ',')) i++;
                if (i >= s.size() || s[i] == ']') break;
                size_t start = i;
                int depth = 0;
                while (i < s.size()) {
                    if (s[i] == '[' || s[i] == '{') depth++;
                    else if (s[i] == ']' || s[i] == '}') { depth--; if (depth < 0) break; }
                    else if (s[i] == ',' && depth == 0) break;
                    i++;
                }
                arr.arrVal->elements.push_back(Value::makeStr(s.substr(start, i - start)));
            }
            return arr;
        }
        // Try number
        char* end = nullptr;
        double n = std::strtod(s.c_str(), &end);
        if (end == s.c_str() + s.length()) return Value::makeNum(n);
        return Value::makeUndefined();
    }));

    jsonObj.setProperty("stringify", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeUndefined();
        return Value::makeStr(args[0].toString());
    }));

    env->define("JSON", jsonObj);

    // Global functions
    env->define("parseInt", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        std::string s = args[0].toString();
        int radix = args.size() > 1 ? (int)args[1].toNumber() : 10;
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        char* end = nullptr;
        double r = std::strtol(s.c_str(), &end, radix);
        if (end == s.c_str()) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        return Value::makeNum(r);
    }));

    env->define("parseFloat", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        std::string s = args[0].toString();
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        char* end = nullptr;
        double r = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        return Value::makeNum(r);
    }));

    env->define("isNaN", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeBool(true);
        return Value::makeBool(std::isnan(args[0].toNumber()));
    }));

    env->define("isFinite", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeBool(false);
        double v = args[0].toNumber();
        return Value::makeBool(std::isfinite(v));
    }));

    env->define("typeof", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeStr("undefined");
        return args[0].typeOf();
    }));

    // String global
    Value stringObj = Value::makeObj();
    stringObj.setProperty("fromCharCode", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        std::string r;
        for (const auto& a : args) r += (char)(int)a.toNumber();
        return Value::makeStr(r);
    }));
    stringObj.setProperty("fromCodePoint", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        std::string r;
        for (const auto& a : args) r += (char)(int)a.toNumber();
        return Value::makeStr(r);
    }));
    env->define("String", stringObj);

    // Number
    Value numberObj = Value::makeObj();
    numberObj.setProperty("MAX_VALUE", Value::makeNum(DBL_MAX));
    numberObj.setProperty("MIN_VALUE", Value::makeNum(DBL_MIN));
    numberObj.setProperty("NaN", Value::makeNum(std::numeric_limits<double>::quiet_NaN()));
    numberObj.setProperty("POSITIVE_INFINITY", Value::makeNum(INFINITY));
    numberObj.setProperty("NEGATIVE_INFINITY", Value::makeNum(-INFINITY));
    numberObj.setProperty("MAX_SAFE_INTEGER", Value::makeNum(9007199254740991));
    numberObj.setProperty("MIN_SAFE_INTEGER", Value::makeNum(-9007199254740991));
    numberObj.setProperty("EPSILON", Value::makeNum(2.220446049250313e-16));
    numberObj.setProperty("isNaN", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeBool(args.empty() ? true : std::isnan(args[0].toNumber()));
    }));
    numberObj.setProperty("isFinite", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeBool(!args.empty() && std::isfinite(args[0].toNumber()));
    }));
    numberObj.setProperty("isInteger", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeBool(false);
        double v = args[0].toNumber();
        return Value::makeBool(std::isfinite(v) && std::floor(v) == v);
    }));
    numberObj.setProperty("isSafeInteger", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeBool(false);
        double v = args[0].toNumber();
        return Value::makeBool(std::isfinite(v) && std::floor(v) == v && std::abs(v) <= 9007199254740991);
    }));
    env->define("Number", numberObj);

    // Boolean
    env->define("Boolean", Value::makeObj());
    // Array
    Value arrayObj = Value::makeObj();
    arrayObj.setProperty("isArray", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeBool(!args.empty() && args[0].type == ValueType::Array);
    }));
    arrayObj.setProperty("from", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value arr = Value::makeArr();
        if (!args.empty() && args[0].isArray()) {
            arr.arrVal->elements = args[0].arrVal->elements;
        }
        return arr;
    }));
    arrayObj.setProperty("of", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value arr = Value::makeArr();
        arr.arrVal->elements = args;
        return arr;
    }));
    env->define("Array", arrayObj);

    // Object
    Value objectObj = Value::makeObj();
    objectObj.setProperty("keys", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value arr = Value::makeArr();
        if (!args.empty() && args[0].type == ValueType::Object) {
            for (const auto& [key, _] : args[0].objVal->properties) {
                arr.arrVal->elements.push_back(Value::makeStr(key));
            }
        }
        return arr;
    }));
    objectObj.setProperty("values", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value arr = Value::makeArr();
        if (!args.empty() && args[0].type == ValueType::Object) {
            for (const auto& [_, val] : args[0].objVal->properties) {
                arr.arrVal->elements.push_back(val);
            }
        }
        return arr;
    }));
    objectObj.setProperty("entries", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value arr = Value::makeArr();
        if (!args.empty() && args[0].type == ValueType::Object) {
            for (const auto& [key, val] : args[0].objVal->properties) {
                Value entry = Value::makeArr();
                entry.arrVal->elements.push_back(Value::makeStr(key));
                entry.arrVal->elements.push_back(val);
                arr.arrVal->elements.push_back(entry);
            }
        }
        return arr;
    }));
    objectObj.setProperty("assign", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeObj();
        Value target = args[0];
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i].type == ValueType::Object) {
                for (const auto& [key, val] : args[i].objVal->properties) {
                    target.setProperty(key, val);
                }
            }
        }
        return target;
    }));
    objectObj.setProperty("create", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeObj();
    }));
    objectObj.setProperty("defineProperty", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.size() >= 3 && args[0].type == ValueType::Object) {
            const_cast<Value&>(args[0]).setProperty(args[1].toString(), args[2]);
        }
        return args.empty() ? Value::makeUndefined() : args[0];
    }));
    // Object.prototype
    Value objectProto = Value::makeObj();
    objectProto.setProperty("toString", Value::makeNative([](const std::vector<Value>&, const Value&) -> Value {
        return Value::makeStr("[object Object]");
    }));
    objectProto.setProperty("hasOwnProperty", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeBool(false);
        std::string prop = args[0].toString();
        return Value::makeBool(true); // simplified
    }));
    gObjectPrototype = objectProto.objVal;
    objectObj.setProperty("prototype", objectProto);

    // Update Object.create to actually set prototype
    objectObj.setProperty("create", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value obj = Value::makeObj();
        if (!args.empty() && args[0].type == ValueType::Object) {
            obj.objVal->proto = args[0].objVal;
        }
        return obj;
    }));

    env->define("Object", objectObj);

    // Function.prototype
    Value functionProto = Value::makeObj();
    if (gObjectPrototype) functionProto.objVal->proto = gObjectPrototype;
    functionProto.setProperty("call", Value::makeNative([](const std::vector<Value>& args, const Value& thisVal) -> Value {
        if (!thisVal.isFunction()) return Value::makeUndefined();
        Value newThis = args.empty() ? Value::makeUndefined() : args[0];
        std::vector<Value> callArgs;
        if (args.size() > 1) callArgs.assign(args.begin() + 1, args.end());
        if (g_callFunction) return g_callFunction(thisVal, callArgs, newThis);
        return Value::makeUndefined();
    }));
    functionProto.setProperty("apply", Value::makeNative([](const std::vector<Value>& args, const Value& thisVal) -> Value {
        if (!thisVal.isFunction()) return Value::makeUndefined();
        Value newThis = args.empty() ? Value::makeUndefined() : args[0];
        std::vector<Value> callArgs;
        if (args.size() > 1 && args[1].isArray()) callArgs = args[1].arrVal->elements;
        if (g_callFunction) return g_callFunction(thisVal, callArgs, newThis);
        return Value::makeUndefined();
    }));
    functionProto.setProperty("bind", Value::makeNative([](const std::vector<Value>& args, const Value& thisVal) -> Value {
        if (!thisVal.isFunction()) return Value::makeUndefined();
        Value boundThis = args.empty() ? Value::makeUndefined() : args[0];
        std::vector<Value> boundArgs;
        if (args.size() > 1) boundArgs.assign(args.begin() + 1, args.end());
        return Value::makeNative([thisVal, boundThis, boundArgs](const std::vector<Value>& callArgs, const Value&) -> Value {
            std::vector<Value> allArgs = boundArgs;
            allArgs.insert(allArgs.end(), callArgs.begin(), callArgs.end());
            if (g_callFunction) return g_callFunction(thisVal, allArgs, boundThis);
            return Value::makeUndefined();
        });
    }));

    // Give Function.prototype to all functions via a global
    gFunctionPrototype = functionProto.objVal;

    // Date
    Value dateObj = Value::makeObj();
    dateObj.setProperty("now", Value::makeNative([](const std::vector<Value>&, const Value&) -> Value {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        return Value::makeNum((double)ms);
    }));
    dateObj.setProperty("parse", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        if (args.empty()) return Value::makeNum(std::numeric_limits<double>::quiet_NaN());
        return Value::makeNum((double)std::time(nullptr) * 1000);
    }));
    env->define("Date", dateObj);

    // RegExp dummy
    env->define("RegExp", Value::makeObj());

    // Error
    Value errorObj = Value::makeObj();
    errorObj.setProperty("Error", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        Value e = Value::makeObj();
        e.setProperty("name", Value::makeStr("Error"));
        e.setProperty("message", args.empty() ? Value::makeStr("") : args[0]);
        return e;
    }));
    env->define("Error", errorObj);

    // globalThis
    Value globalObj = Value::makeObj();
    globalObj.setProperty("globalThis", globalObj);
    env->define("globalThis", globalObj);

    // NaN, Infinity, undefined constants
    env->define("NaN", Value::makeNum(std::numeric_limits<double>::quiet_NaN()), true);
    env->define("Infinity", Value::makeNum(INFINITY), true);
    env->define("undefined", Value::makeUndefined(), true);

    // Global `this` - use the global object
    env->define("this", globalObj);

    // setTimeout dummy
    env->define("setTimeout", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(0);
    }));

    // setInterval dummy
    env->define("setInterval", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return Value::makeNum(0);
    }));

    // clearTimeout
    env->define("clearTimeout", Value::makeNative([](const std::vector<Value>&, const Value&) -> Value {
        return Value::makeUndefined();
    }));

    // clearInterval
    env->define("clearInterval", Value::makeNative([](const std::vector<Value>&, const Value&) -> Value {
        return Value::makeUndefined();
    }));

    // encodeURI / decodeURI
    env->define("encodeURI", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return args.empty() ? Value::makeStr("") : Value::makeStr(args[0].toString());
    }));
    env->define("decodeURI", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return args.empty() ? Value::makeStr("") : Value::makeStr(args[0].toString());
    }));
    env->define("encodeURIComponent", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return args.empty() ? Value::makeStr("") : Value::makeStr(args[0].toString());
    }));
    env->define("decodeURIComponent", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return args.empty() ? Value::makeStr("") : Value::makeStr(args[0].toString());
    }));

    // eval - just returns the expression
    env->define("eval", Value::makeNative([](const std::vector<Value>& args, const Value&) -> Value {
        return args.empty() ? Value::makeUndefined() : Value::makeStr(args[0].toString());
    }));
}

}
