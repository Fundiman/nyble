#include "env.h"

namespace nyble {

void GCFunction::trace(std::vector<GCHeader*>& worklist) {
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
