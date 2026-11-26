#include "ant_engine.h"

#include "ant_engine_c.h"

#include <cstdlib>
#include <stdexcept>

namespace nyble {

struct Engine::Impl {
    nyble_engine *engine = nullptr;
};

Engine::Engine(int argc, char **argv, size_t heapBytes, bool nodeCompat) : impl_(new Impl()) {
    impl_->engine = nyble_engine_create(argc, argv, heapBytes, nodeCompat ? 1 : 0);
    if (!impl_->engine) {
        delete impl_;
        impl_ = nullptr;
        throw std::runtime_error("Ant engine could not be created");
    }
}

Engine::~Engine() {
    if (impl_) {
        if (impl_->engine) nyble_engine_destroy(impl_->engine);
        delete impl_;
        impl_ = nullptr;
    }
}

bool Engine::eval(const std::string &source, std::string &result) {
    errors_.clear();
    result.clear();

    char *out = nullptr;
    const bool ok = nyble_engine_eval(impl_->engine, source.c_str(), source.size(), &out) != 0;
    if (out) {
        if (ok) {
            result = out;
        } else {
            errors_.push_back(out);
        }
        std::free(out);
    } else if (!ok) {
        errors_.push_back("JavaScript error");
    }
    return ok;
}

void Engine::setProp(const std::string &key, const std::string &value) {
    if (impl_ && impl_->engine) {
        nyble_engine_set_prop(impl_->engine, key.c_str(), value.c_str());
    }
}

void Engine::drainEventLoop() {
    if (impl_ && impl_->engine) nyble_engine_drain(impl_->engine);
}

void *Engine::rawIsolate() const {
    return impl_ && impl_->engine ? nyble_engine_isolate(impl_->engine) : nullptr;
}

} // namespace nyble
