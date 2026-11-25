#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_set>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "cats.h"
#include "engine/ant_engine.h"

#ifndef NYBLE_VM_THRESHOLD
#define NYBLE_VM_THRESHOLD 300
#endif

static const char* NYBLE_VERSION = "0.7";
static const char* NYBLE_VERSION_NAME = "AttentionIsAllYouNeed";

// ANSI color codes
static const char* C_RESET   = "\033[0m";
static const char* C_BOLD    = "\033[1m";
static const char* C_DIM     = "\033[2m";
static const char* C_ITALIC  = "\033[3m";
static const char* C_RED     = "\033[1;31m";
static const char* C_GREEN   = "\033[1;32m";
static const char* C_YELLOW  = "\033[1;33m";
static const char* C_ORANGE  = "\033[1;38;5;208m";
static const char* C_CYAN    = "\033[1;36m";
static const char* C_WHITE   = "\033[1;37m";
static const char* C_MAGENTA = "\033[1;35m";
static const char* C_GRAY    = "\033[90m";

enum class EngineMode { Auto, TreeWalk, BytecodeVM, JIT };

static void printMeow() {
    Entry e = get_wisdom();
    const char* author = e.author;
    const char* text = e.text;
    std::cout << "\n"
              << C_WHITE
              << "  /\\_/\\  \n"
              << " ( o.o ) \n"
              << "  > ^ <  \n"
              << " /|   |\\ \n"
              << "(_|   |_)\n"
              << C_RESET
              << "\n"
              << C_ITALIC << "  " << text << "\n" << C_RESET
              << C_GRAY << "  — " << author << C_RESET << "\n"
              << "\n";
}

static void printHelp() {
    std::cout << C_CYAN << C_BOLD << "Usage:" << C_RESET << " " << C_WHITE << "njs" << C_RESET << " [options] <file>\n"
              << "       " << C_WHITE << "njs" << C_RESET << " [options]          " << C_GRAY << "(REPL mode)" << C_RESET << "\n"
              << "\n"
              << C_CYAN << C_BOLD << "Options:" << C_RESET << "\n"
              << "  " << C_YELLOW << "-help, --help" << C_RESET << "          Show this help message\n"
              << "  " << C_YELLOW << "-version, --version" << C_RESET << "    Show version info\n"
              << "  " << C_YELLOW << "-Xmx<size>" << C_RESET << "             Set GC max memory budget " << C_DIM << "(e.g. 256m, 1g)" << C_RESET << "\n"
              << "  " << C_YELLOW << "-engine, --engine <type>" << C_RESET << "  Force execution engine:\n"
              << "                            " << C_GREEN << "tree" << C_RESET << "    - Tree-walking interpreter only\n"
              << "                            " << C_GREEN << "vm" << C_RESET << "      - Bytecode VM only\n"
              << "                            " << C_GREEN << "jit" << C_RESET << "      - JIT compiler " << C_DIM << "(not yet implemented)" << C_RESET << "\n"
              << "  " << C_YELLOW << "-c <code>" << C_RESET << "              Execute inline JavaScript code\n"
              << "  " << C_YELLOW << "-D<key>=<value>" << C_RESET << "        Set a custom property " << C_DIM << "(accessible via Nyble.props)" << C_RESET << "\n"
              << "\n"
              << C_CYAN << C_BOLD << "Examples:" << C_RESET << "\n"
              << "  " << C_WHITE << "njs" << C_RESET << " script.js\n"
              << "  " << C_WHITE << "njs" << C_RESET << " -engine tree script.js\n"
              << "  " << C_WHITE << "njs" << C_RESET << " -Xmx512m -Dname=app script.js\n"
              << "  " << C_WHITE << "njs" << C_RESET << " --engine vm -Xmx1g script.js\n"
              << "  " << C_WHITE << "njs" << C_RESET << " -c \"console.log(42)\"\n"
              << "  " << C_WHITE << "njs" << C_RESET << " --meow\n"
              << "\n"
              << C_MAGENTA << C_BOLD << "This NJS has Super Cat Powers." << C_RESET << "\n";
}

static void printVersion() {
    std::cout << C_ORANGE << "Nyble" << C_YELLOW << "JS " << C_RESET << C_WHITE << "v" << NYBLE_VERSION << C_RESET << " " << C_GRAY << "(" << NYBLE_VERSION_NAME << ")" << C_RESET << "\n";
}

struct CliOptions {
    EngineMode engine = EngineMode::Auto;
    size_t memBytes = 0;
    bool memSet = false;
    bool showHelp = false;
    bool showVersion = false;
    bool showMeow = false;
    std::string filename;
    std::string code;
    std::map<std::string, std::string> props;
};

static CliOptions parseArgs(int argc, char* argv[]) {
    CliOptions opts;
    int i = 1;

    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "-help" || arg == "--help") {
            opts.showHelp = true;
            i++;
        } else if (arg == "-version" || arg == "--version") {
            opts.showVersion = true;
            i++;
        } else if (arg == "-meow" || arg == "--meow") {
            opts.showMeow = true;
            i++;
        } else if (arg.substr(0, 4) == "-Xmx") {
            std::string sizeStr = arg.substr(4);
            size_t multiplier = 1;
            if (!sizeStr.empty()) {
                char last = sizeStr.back();
                if (last == 'm' || last == 'M') {
                    multiplier = 1024ULL * 1024ULL;
                    sizeStr.pop_back();
                } else if (last == 'g' || last == 'G') {
                    multiplier = 1024ULL * 1024ULL * 1024ULL;
                    sizeStr.pop_back();
                } else if (last == 'k' || last == 'K') {
                    multiplier = 1024ULL;
                    sizeStr.pop_back();
                }
            }
            opts.memBytes = std::stoull(sizeStr) * multiplier;
            opts.memSet = true;
            i++;
        } else if (arg == "-engine" || arg == "--engine") {
            if (i + 1 >= argc) {
            std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << arg << " requires a value (tree, vm, jit)" << C_RESET << "\n";
                exit(1);
            }
            std::string val = argv[i + 1];
            if (val == "tree") opts.engine = EngineMode::TreeWalk;
            else if (val == "vm") opts.engine = EngineMode::BytecodeVM;
            else if (val == "jit") opts.engine = EngineMode::JIT;
            else {
                std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "Unknown engine '" << val << "'. Use tree, vm, or jit." << C_RESET << "\n";
                exit(1);
            }
            i += 2;
        } else if (arg == "-c") {
            if (i + 1 >= argc) {
                std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "-c requires a code string" << C_RESET << "\n";
                exit(1);
            }
            opts.code = argv[i + 1];
            i += 2;
        } else if (arg.substr(0, 2) == "-D") {
            std::string prop = arg.substr(2);
            auto eq = prop.find('=');
            if (eq == std::string::npos) {
                std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "Invalid property format. Use -Dkey=value" << C_RESET << "\n";
                exit(1);
            }
            opts.props[prop.substr(0, eq)] = prop.substr(eq + 1);
            i++;
        } else if (arg[0] == '-') {
            std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "Unknown option '" << arg << "'. Use -help for usage." << C_RESET << "\n";
            exit(1);
        } else {
            opts.filename = arg;
            i++;
        }
    }

    return opts;
}

[[maybe_unused]] static size_t detectSystemMemoryMB() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return (size_t)(status.ullTotalPhys / (1024 * 1024));
#endif
    return 2048;
}

static bool isKeyword(const std::string& word) {
    static const std::unordered_set<std::string> kw = {
        "let","const","var","function","if","else","while","for","do",
        "return","break","continue","switch","case","default",
        "true","false","null","undefined","typeof","new","this",
        "class","of","in","try","catch","finally","throw"
    };
    return kw.count(word) > 0;
}

static void printHighlighted(const std::string& input) {
    size_t i = 0, len = input.size();
    while (i < len) {
        char c = input[i];

        if (c == '\'' || c == '"' || c == '`') {
            char delim = c;
            std::cout << C_GREEN << c; i++;
            while (i < len && input[i] != delim) {
                if (input[i] == '\\' && i + 1 < len) { std::cout << input[i] << input[i+1]; i += 2; }
                else { std::cout << input[i]; i++; }
            }
            if (i < len) { std::cout << input[i]; i++; }
            std::cout << C_RESET;
            continue;
        }

        if (std::isdigit(c) || (c == '.' && i + 1 < len && std::isdigit(input[i+1]))) {
            std::cout << C_CYAN;
            while (i < len && (std::isdigit(input[i]) || input[i] == '.' || input[i] == 'e' || input[i] == 'E' || input[i] == '+' || input[i] == '-')) {
                std::cout << input[i]; i++;
            }
            std::cout << C_RESET;
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            std::string word;
            while (i < len && (std::isalnum(input[i]) || input[i] == '_')) { word += input[i]; i++; }
            if (isKeyword(word)) {
                if (word == "true" || word == "false" || word == "null" || word == "undefined")
                    std::cout << C_YELLOW << C_BOLD << word << C_RESET;
                else
                    std::cout << C_MAGENTA << word << C_RESET;
            } else {
                std::cout << C_WHITE << word << C_RESET;
            }
            continue;
        }

        if (std::string("+-*/%=<>!&|^~?:").find(c) != std::string::npos) {
            std::cout << C_RED << c << C_RESET; i++;
            continue;
        }

        if (std::string("(){}[].,;").find(c) != std::string::npos) {
            std::cout << C_GRAY << c << C_RESET; i++;
            continue;
        }

        std::cout << c; i++;
    }
}

#ifdef _WIN32
static HANDLE hStdin = nullptr;
static DWORD origConsoleMode = 0;

static bool stdinIsTTY() {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    return h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode) != 0;
}

static void enableRawMode() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &origConsoleMode);
    SetConsoleMode(hStdin, origConsoleMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
}

static void disableRawMode() {
    if (hStdin) SetConsoleMode(hStdin, origConsoleMode);
}

static int readKey() {
    INPUT_RECORD rec;
    DWORD count;
    while (true) {
        if (!ReadConsoleInput(hStdin, &rec, 1, &count)) return -1;
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            if (rec.Event.KeyEvent.uChar.AsciiChar != 0)
                return rec.Event.KeyEvent.uChar.AsciiChar;
            switch (rec.Event.KeyEvent.wVirtualKeyCode) {
                case VK_UP:    return 0x100;
                case VK_DOWN:  return 0x101;
                case VK_LEFT:  return 0x102;
                case VK_RIGHT: return 0x103;
                case VK_HOME:  return 0x104;
                case VK_END:   return 0x105;
                case VK_DELETE: return 0x106;
                default: break;
            }
        }
    }
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    CliOptions opts = parseArgs(argc, argv);

    if (opts.showHelp) {
        printHelp();
        return 0;
    }
    if (opts.showVersion) {
        printVersion();
        return 0;
    }
    if (opts.showMeow) {
        printMeow();
        return 0;
    }
    if (opts.engine == EngineMode::JIT) {
        std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "JIT compiler is not yet implemented." << C_RESET << "\n";
        return 1;
    }

    (void)detectSystemMemoryMB; // kept for potential future diagnostics

    if (!opts.filename.empty() || !opts.code.empty()) {
        std::string source;
        if (!opts.code.empty()) {
            source = opts.code;
        } else {
            std::ifstream file(opts.filename);
            if (!file.is_open()) {
                std::cerr << C_RED << C_BOLD << "Error: " << C_RESET << C_RED << "Cannot open file '" << opts.filename << "'" << C_RESET << "\n";
                return 1;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            source = buffer.str();
        }

        try {
            nyble::Engine engine(argc, argv, opts.memSet ? opts.memBytes : 0);
            for (const auto& kv : opts.props) engine.setProp(kv.first, kv.second);

            std::string result;
            const bool ok = engine.eval(source, result);
            engine.drainEventLoop();
            if (!ok) {
                for (const auto& err : engine.errors()) {
                    std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << err << C_RESET << "\n";
                }
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << C_RED << C_BOLD << "Internal Error: " << C_RESET << C_RED << e.what() << C_RESET << "\n";
            return 1;
        }

        return 0;
    }

    // REPL - interactive (TTY) and piped (non-TTY) modes
    nyble::Engine engine(argc, argv, opts.memSet ? opts.memBytes : 0);
    for (const auto& kv : opts.props) engine.setProp(kv.first, kv.second);

    bool piped = !stdinIsTTY();

    if (!piped) {
        std::cout << C_ORANGE << "Nyble" << C_YELLOW << "JS " << C_RESET << C_WHITE << "v" << NYBLE_VERSION << C_RESET << " " << C_GRAY << "(" << NYBLE_VERSION_NAME << ")" << C_RESET << " - JavaScript Engine\n";
        std::cout << C_DIM << "Type 'exit' to quit" << C_RESET << "\n\n";
        enableRawMode();
    }

    std::string source;

    auto rerender = [&](const std::string& prompt, const std::string& buf, size_t cursor) {
        std::cout << "\r" << prompt;
        printHighlighted(buf);
        std::cout << "\033[K";
        size_t target = prompt.size() + cursor;
        if (target < prompt.size() + buf.size())
            std::cout << "\r\033[" << target << "C";
        std::cout << std::flush;
    };

    // Heuristic: unfinished multi-line construct? (Ant handles ASI; this just
    // keeps the old REPL feel for { / ( / [ blocks and trailing operators.)
    auto looksIncomplete = [](const std::string& src) -> bool {
        int braces = 0, parens = 0, brackets = 0;
        bool inStr = false, inComment = false;
        char strCh = 0;
        for (size_t i = 0; i < src.size(); i++) {
            char c = src[i];
            if (inComment) {
                if (c == '\n') inComment = false;
                continue;
            }
            if (i + 1 < src.size() && c == '/' && src[i+1] == '/') { inComment = true; i++; continue; }
            if (inStr) {
                if (c == '\\') { i++; continue; }
                if (c == strCh) inStr = false;
                continue;
            }
            if (c == '"' || c == '\'') { inStr = true; strCh = c; continue; }
            switch (c) {
                case '{': braces++; break;
                case '}': braces--; break;
                case '(': parens++; break;
                case ')': parens--; break;
                case '[': brackets++; break;
                case ']': brackets--; break;
            }
        }
        if (braces > 0 || parens > 0 || brackets > 0) return true;
        size_t last = src.find_last_not_of(" \t\r\n");
        if (last != std::string::npos) {
            char lc = src[last];
            if (std::string("+-*/,.").find(lc) != std::string::npos) return true;
        }
        return false;
    };

    while (true) {
        std::string lineBuffer;
        size_t cursorPos = 0;

        if (piped) {
            if (!std::getline(std::cin, lineBuffer)) break;
        } else {
            std::string prompt = source.empty() ? "> " : "... ";
            std::cout << prompt << std::flush;
            rerender(prompt, lineBuffer, cursorPos);

            while (true) {
                int ch = readChar();
                if (ch == -1) { disableRawMode(); return 0; }
                if (ch == 3) { // Ctrl+C
                    std::cout << "\n" << prompt << std::flush;
                    lineBuffer.clear();
                    cursorPos = 0;
                    rerender(prompt, lineBuffer, cursorPos);
                    continue;
                }
                if (ch == 4) { // Ctrl+D
                    disableRawMode();
                    std::cout << "\n" << std::flush;
                    return 0;
                }

                if (ch == '\r' || ch == '\n') {
                    std::cout << "\n" << std::flush;
                    break;
                }

                if (ch == 127 || ch == 8) { // Backspace
                    if (cursorPos > 0) {
                        lineBuffer.erase(cursorPos - 1, 1);
                        cursorPos--;
                    }
                } else if (ch == 1) { // Ctrl+A - home
                    cursorPos = 0;
                } else if (ch == 5) { // Ctrl+E - end
                    cursorPos = lineBuffer.size();
                } else if (ch == 11) { // Ctrl+K - kill to end
                    lineBuffer.resize(cursorPos);
                } else if (ch == 21) { // Ctrl+U - kill to start
                    lineBuffer.erase(0, cursorPos);
                    cursorPos = 0;
                } else if (ch == 23) { // Ctrl+W - delete word back
                    while (cursorPos > 0 && lineBuffer[cursorPos - 1] == ' ') { lineBuffer.erase(cursorPos - 1, 1); cursorPos--; }
                    while (cursorPos > 0 && lineBuffer[cursorPos - 1] != ' ') { lineBuffer.erase(cursorPos - 1, 1); cursorPos--; }
                } else if (ch == 0x100) { // Up
                } else if (ch == 0x101) { // Down
                } else if (ch == 0x102) { // Left
                    if (cursorPos > 0) cursorPos--;
                } else if (ch == 0x103) { // Right
                    if (cursorPos < lineBuffer.size()) cursorPos++;
                } else if (ch == 0x104) { // Home
                    cursorPos = 0;
                } else if (ch == 0x105) { // End
                    cursorPos = lineBuffer.size();
                } else if (ch == 0x106) { // Delete
                    if (cursorPos < lineBuffer.size()) lineBuffer.erase(cursorPos, 1);
                } else if (ch >= 32 && ch < 127) {
                    lineBuffer.insert(lineBuffer.begin() + cursorPos, (char)ch);
                    cursorPos++;
                }

                rerender(prompt, lineBuffer, cursorPos);
            }

            if (source.empty() && lineBuffer == "exit") {
                disableRawMode();
                break;
            }
        }

        if (lineBuffer.empty() && source.empty()) continue;

        source += lineBuffer + "\n";

        std::string result;
        const bool ok = engine.eval(source, result);
        engine.drainEventLoop();

        if (!ok) {
            if (looksIncomplete(source)) {
                continue; // wait for more lines
            }
            for (const auto& err : engine.errors()) {
                std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << err << C_RESET << "\n";
            }
            source.clear();
            continue;
        }

        if (!result.empty()) std::cout << result << "\n";
        source.clear();
    }

    return 0;
}
