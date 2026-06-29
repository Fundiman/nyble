#pragma once
#include <cstddef>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdexcept>
#include <new>

namespace nyble {

enum class GCType {
    String, Object, Array, Function
};

struct GCHeader {
    GCHeader* next;
    GCType type;
    bool marked;
    GCHeader(GCType t) : next(nullptr), type(t), marked(false) {}
    virtual ~GCHeader() = default;
    virtual void trace(std::vector<GCHeader*>& worklist) {}
    virtual size_t approxSize() const { return sizeof(GCHeader); }
};

struct GCString : GCHeader {
    std::string str;
    GCString(const std::string& s) : GCHeader(GCType::String), str(s) {}
    size_t approxSize() const override { return sizeof(GCString) + str.capacity(); }
};

class GCHeap {
    GCHeader* head;
    int allocCount;
    int nextGC;
    size_t totalBytes;
    size_t memBudget;
    std::vector<GCHeader*> tempRoots;

    void sweep();

    static constexpr size_t defaultBudget = 256 * 1024 * 1024;

public:
    std::function<void(std::vector<GCHeader*>&)> rootTracer;

    GCHeap() : head(nullptr), allocCount(0), nextGC(1024), totalBytes(0), memBudget(defaultBudget) {}
    ~GCHeap();

    void setMemoryBudget(size_t bytes) { memBudget = bytes; }

    void pin(GCHeader* h) {
        if (h) tempRoots.push_back(h);
    }
    void unpin(GCHeader* h) {
        for (size_t i = 0; i < tempRoots.size(); i++) {
            if (tempRoots[i] == h) {
                tempRoots[i] = tempRoots.back();
                tempRoots.pop_back();
                return;
            }
        }
    }

    void checkBudget(size_t est) {
        if (totalBytes + est > memBudget) {
            memBudget = totalBytes + est + (memBudget / 2);
        }
    }

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj;
        try {
            obj = new T(std::forward<Args>(args)...);
        } catch (const std::bad_alloc&) {
            throw std::runtime_error("Out of memory: GC allocation limit reached");
        }
        size_t est = obj->approxSize();
        checkBudget(est);
        obj->next = head;
        head = obj;
        allocCount++;
        totalBytes += est;
        return obj;
    }

    void collect();
};

inline GCHeap gHeap;

struct GCPin {
    GCHeader* h;
    GCPin(GCHeader* h) : h(h) { if (h) gHeap.pin(h); }
    ~GCPin() { if (h) gHeap.unpin(h); }
    GCPin(const GCPin&) = delete;
    GCPin& operator=(const GCPin&) = delete;
};

}
