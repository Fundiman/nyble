#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include "lexer.h"
#include "parser.h"
#include "interp.h"

int main(int argc, char* argv[]) {
    nyble::Interpreter interp;

    if (argc > 1) {
        std::string filename = argv[1];
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << filename << "'\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        nyble::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        nyble::Parser parser(tokens);
        auto program = parser.parse();

        if (!parser.getErrors().empty()) {
            for (const auto& err : parser.getErrors()) {
                std::cerr << err << "\n";
            }
            return 1;
        }

        try {
            interp.evaluate(program);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    // REPL
    std::cout << "NybleJS v0.1.0 - JavaScript Engine\n";
    std::cout << "Type 'exit' to quit\n\n";

    std::string line;
    std::string source;

    while (true) {
        if (source.empty()) {
            std::cout << "> ";
        } else {
            std::cout << "  ";
        }

        if (!std::getline(std::cin, line)) break;

        if (source.empty() && line == "exit") break;

        source += line + "\n";

        // Try to parse and execute
        nyble::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        nyble::Parser parser(tokens);
        auto program = parser.parse();

        if (!parser.getErrors().empty()) {
            // Check if it's just incomplete input
            bool incomplete = false;
            for (const auto& err : parser.getErrors()) {
                if (err.find("Expected") != std::string::npos) {
                    incomplete = true;
                }
            }
            if (incomplete && source.find(';') == std::string::npos) {
                continue; // More input needed
            }
        }

        try {
            auto result = interp.evaluate(program);
            if (result.type != nyble::ValueType::Undefined && result.type != nyble::ValueType::Null) {
                std::cout << result.toString() << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }

        source.clear();
    }

    return 0;
}
