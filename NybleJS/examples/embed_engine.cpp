// Minimal v1 embedding example: nyble::Engine over Ant Silver.
// Build: compile this file + src/engine/ant_engine.cpp (C++17) and
//        src/engine/ant_engine.c (C), then link libant.a. See README.md.
//
// Node.js compatibility is OFF by default for embedders. Pass true as the
// fourth constructor argument only if you need Buffer/fs/process/tty/events.

#include <iostream>
#include <string>

#include "src/nyblejs.h"

int main() {
    nyble::Engine engine(0, nullptr, 0, /*nodeCompat=*/false);

    // Same mechanism as CLI -Dkey=value: strings on Nyble.props.
    engine.setProp("app", "embed-engine");

    std::string out;
    if (!engine.eval("Nyble.props.app + ': ' + (1 + 1)", out)) {
        for (const auto& err : engine.errors())
            std::cerr << "Uncaught " << err << '\n';
        return 1;
    }

    std::cout << out << '\n';  // embed-engine: 2
    engine.drainEventLoop();   // pending promise jobs / timers
    return 0;
}
