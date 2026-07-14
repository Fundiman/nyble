#include <iostream>
#include "src/nyblejs.h"

int main() {
    std::string source = "console.log('hello from linked njs');";

    nyble::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    nyble::Parser parser(tokens);
    auto program = parser.parse();

    if (!parser.getErrors().empty()) {
        for (auto& err : parser.getErrors())
            std::cerr << err << "\n";
        return 1;
    }

    nyble::BytecodeChunk chunk;
    nyble::Compiler comp(&chunk);
    comp.compile(program);
    nyble::VM vm;
    nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
        if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
    };
    auto result = vm.run(&chunk, vm.globalEnv);
    std::cout << "result: " << result.toString() << "\n";
    return 0;
}
