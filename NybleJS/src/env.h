#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "value.h"

namespace nyble {

struct GCHeader;

class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> values;
    std::unordered_map<std::string, bool> constants;

    Environment() : parent(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> p) : parent(std::move(p)) {}

    Environment* root() {
        auto e = this;
        while (e->parent) e = e->parent.get();
        return e;
    }

    void define(const std::string& name, const Value& val, bool isConst = false) {
        values[name] = val;
        if (isConst) constants[name] = true;
    }

    bool isConstant(const std::string& name) const {
        auto it = constants.find(name);
        return it != constants.end() && it->second;
    }

    Value get(const std::string& name) const {
        auto it = values.find(name);
        if (it != values.end()) return it->second;
        if (parent) return parent->get(name);
        return Value::makeUndefined();
    }

    bool exists(const std::string& name) const {
        if (values.find(name) != values.end()) return true;
        if (parent) return parent->exists(name);
        return false;
    }

    Value set(const std::string& name, const Value& val) {
        if (values.find(name) != values.end()) {
            if (isConstant(name)) {
                throw std::runtime_error("Cannot assign to constant '" + name + "'");
            }
            values[name] = val;
            return val;
        }
        if (parent) return parent->set(name, val);
        values[name] = val;
        return val;
    }

    std::shared_ptr<Environment> createChild() {
        return std::make_shared<Environment>(shared_from_this());
    }

    void traceGCValues(std::vector<GCHeader*>& worklist) {
        for (auto& [key, val] : values) {
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
        if (parent) {
            parent->traceGCValues(worklist);
        }
    }
};

inline void GCFunction::trace(std::vector<GCHeader*>& worklist) {
    if (closure) {
        for (auto& [key, val] : closure->values) {
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
    }
}

}
