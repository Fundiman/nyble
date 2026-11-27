#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace nyble {

// Stable façade over the Ant Silver engine (libant).
// All Ant types stay inside the C bridge; the rest of Nyble deals in strings.
// One Engine == one Ant isolate == one JS global scope.
class Engine {
public:
    // Node compat is OFF by default for embeddings (web-first). Pass true to
    // opt into Buffer/fs/process/tty/events and node: module resolution.
    Engine(int argc = 0, char** argv = nullptr, size_t heapBytes = 0, bool nodeCompat = false);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Evaluate `source` in the isolate's global scope.
    // Success: returns true, `result` holds the completion value rendered for
    //          display (empty for undefined/null).
    // Failure: returns false and fills errors() with the exception text.
    bool eval(const std::string& source, std::string& result);

    // Define Nyble.props.<key> = <value> for `-D<key>=<value>`.
    void setProp(const std::string& key, const std::string& value);

    // Pending promise jobs / timers. Call after a script finishes so async
    // work gets a chance to run before the process exits.
    void drainEventLoop();

    const std::vector<std::string>& errors() const { return errors_; }

    // Escape hatch for host bindings (DOM, storage...). Returns ant_t*.
    void* rawIsolate() const;

private:
    struct Impl;
    Impl* impl_;
    std::vector<std::string> errors_;
};

} // namespace nyble
