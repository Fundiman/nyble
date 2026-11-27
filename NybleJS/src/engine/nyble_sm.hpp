#pragma once

// C++ convenience layer over the SpiderMonkey-style C API.
// Think JSRuntime / JSContext / JS::RootedValue, not V8 Local/Persistent.

#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "nyble_sm.h"

namespace nyble {
namespace sm {

class Context;

class RootedValue {
public:
    RootedValue() noexcept : cx_(nullptr), value_(NY_VALUE_INVALID) {}
    RootedValue(Context& cx, NyValue value) noexcept : cx_(&cx), value_(value) {}
    ~RootedValue() { reset(); }

    RootedValue(const RootedValue&) = delete;
    RootedValue& operator=(const RootedValue&) = delete;

    RootedValue(RootedValue&& other) noexcept : cx_(other.cx_), value_(other.value_) {
        other.cx_ = nullptr;
        other.value_ = NY_VALUE_INVALID;
    }

    RootedValue& operator=(RootedValue&& other) noexcept {
        if (this != &other) {
            reset();
            cx_ = other.cx_;
            value_ = other.value_;
            other.cx_ = nullptr;
            other.value_ = NY_VALUE_INVALID;
        }
        return *this;
    }

    bool valid() const noexcept { return value_ != NY_VALUE_INVALID; }
    NyValue get() const noexcept { return value_; }

    std::string toString() const;
    double toNumber() const;
    NyType type() const;

    // Unroot early. Also called by the destructor.
    void reset() noexcept;

private:
    Context* cx_;
    NyValue value_;
};

class Runtime {
public:
    explicit Runtime(size_t heap_bytes = 0, bool node_compat = false)
        : rt_(Ny_NewRuntime(heap_bytes, node_compat ? 1 : 0)) {}

    ~Runtime() { Ny_DestroyRuntime(rt_); }

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    NyRuntime* raw() const noexcept { return rt_; }
    bool valid() const noexcept { return rt_ != nullptr; }

private:
    NyRuntime* rt_;
};

class Context {
public:
    explicit Context(Runtime& rt, int argc = 0, char** argv = nullptr)
        : cx_(Ny_NewContext(rt.raw(), argc, argv)) {}

    ~Context() { Ny_DestroyContext(cx_); }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    NyContext* raw() const noexcept { return cx_; }
    bool valid() const noexcept { return cx_ != nullptr; }

    RootedValue evaluate(const std::string& source) {
        return RootedValue(*this, Ny_Evaluate(cx_, source.data(), source.size()));
    }

    RootedValue undefined() { return RootedValue(*this, Ny_Undefined(cx_)); }
    RootedValue null() { return RootedValue(*this, Ny_Null(cx_)); }
    RootedValue boolean(bool value) { return RootedValue(*this, Ny_Boolean(cx_, value ? 1 : 0)); }
    RootedValue number(double value) { return RootedValue(*this, Ny_Number(cx_, value)); }
    RootedValue string(const std::string& value) {
        return RootedValue(*this, Ny_String(cx_, value.data(), value.size()));
    }

    RootedValue getProperty(const RootedValue& obj, const char* key) {
        NyValue out = NY_VALUE_INVALID;
        if (!cx_ || !obj.valid() || !key) return RootedValue();
        Ny_GetProperty(cx_, obj.get(), key, &out);
        return RootedValue(*this, out);
    }

    void setProperty(const RootedValue& obj, const char* key, const RootedValue& value) {
        if (cx_ && obj.valid() && key && value.valid())
            Ny_SetProperty(cx_, obj.get(), key, value.get());
    }

    RootedValue call(const RootedValue& fn, const RootedValue& this_val,
                     const std::vector<NyValue>& args) {
        NyValue out = NY_VALUE_INVALID;
        if (!cx_ || !fn.valid()) return RootedValue();
        Ny_Call(cx_, fn.get(), this_val.valid() ? this_val.get() : NY_VALUE_INVALID,
                args.empty() ? nullptr : args.data(), args.size(), &out);
        return RootedValue(*this, out);
    }

    RootedValue call(const RootedValue& fn, const RootedValue& this_val,
                     const std::vector<RootedValue>& args) {
        std::vector<NyValue> raw;
        raw.reserve(args.size());
        for (const auto& arg : args) raw.push_back(arg.get());
        return call(fn, this_val, raw);
    }

    std::string errorMessage() const { return cx_ ? Ny_GetErrorMessage(cx_) : ""; }

    bool pendingException(std::string& out) const {
        char* msg = nullptr;
        const bool pending = cx_ && Ny_HasPendingException(cx_, &msg) != 0;
        if (msg) {
            out = msg;
            std::free(msg);
        } else {
            out.clear();
        }
        return pending;
    }

    void clearPendingException() { if (cx_) Ny_ClearPendingException(cx_); }
    void drainJobQueue() { if (cx_) Ny_DrainJobQueue(cx_); }

private:
    NyContext* cx_;
};

inline std::string RootedValue::toString() const {
    if (!cx_ || !valid()) return std::string();
    char* s = Ny_ToString(cx_->raw(), value_);
    std::string out = s ? s : "";
    std::free(s);
    return out;
}

inline double RootedValue::toNumber() const {
    return (cx_ && valid()) ? Ny_ToNumber(cx_->raw(), value_) : 0.0;
}

inline NyType RootedValue::type() const {
    return cx_ ? Ny_TypeOf(cx_->raw(), value_) : NYBLE_T_UNDEFINED;
}

inline void RootedValue::reset() noexcept {
    if (cx_ && valid()) Ny_FreeValue(cx_->raw(), value_);
    cx_ = nullptr;
    value_ = NY_VALUE_INVALID;
}

} // namespace sm
} // namespace nyble
