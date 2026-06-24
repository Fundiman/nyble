#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace nyble {

struct GCAlloc {
    GCAlloc* next;
    bool marked;
    GCAlloc() : next(nullptr), marked(false) {}
    virtual ~GCAlloc() = default;
    virtual void trace(std::vector<GCAlloc*>&) {}
};

}
