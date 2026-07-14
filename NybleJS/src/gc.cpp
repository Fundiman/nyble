#include "gc.h"
#include "value.h"

namespace nyble {

void GCHeap::collect() {
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

void GCHeap::sweep() {
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

GCHeap::~GCHeap() {
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
