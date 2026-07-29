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
#include "nyblejs.h"

#ifndef NYBLE_VM_THRESHOLD
#define NYBLE_VM_THRESHOLD 300
#endif

static const char* NYBLE_VERSION = "0.6";
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

namespace {

size_t countExpr(const nyble::Expr* expr);
size_t countStmt(const nyble::Stmt* stmt);

size_t countExpr(const nyble::Expr* expr) {
    if (!expr) return 0;
    size_t n = 1;
    switch (expr->type) {
        case nyble::ASTType::Binary: {
            auto* b = static_cast<const nyble::BinaryExprNode*>(expr);
            n += countExpr(b->left.get()) + countExpr(b->right.get());
            break;
        }
        case nyble::ASTType::Unary: {
            auto* u = static_cast<const nyble::UnaryExprNode*>(expr);
            n += countExpr(u->operand.get());
            break;
        }
        case nyble::ASTType::Call: {
            auto* c = static_cast<const nyble::CallExprNode*>(expr);
            n += countExpr(c->callee.get());
            for (auto& a : c->args) n += countExpr(a.get());
            break;
        }
        case nyble::ASTType::Member: {
            auto* m = static_cast<const nyble::MemberExprNode*>(expr);
            n += countExpr(m->object.get()) + countExpr(m->property.get());
            break;
        }
        case nyble::ASTType::ArrayLit: {
            auto* a = static_cast<const nyble::ArrayLitNode*>(expr);
            for (auto& e : a->elements) n += countExpr(e.get());
            break;
        }
        case nyble::ASTType::ObjectLit: {
            auto* o = static_cast<const nyble::ObjectLitNode*>(expr);
            for (auto& [k, v] : o->properties) n += countExpr(v.get());
            break;
        }
        case nyble::ASTType::Assignment: {
            auto* a = static_cast<const nyble::AssignNode*>(expr);
            n += countExpr(a->target.get()) + countExpr(a->value.get());
            break;
        }
        case nyble::ASTType::Conditional: {
            auto* c = static_cast<const nyble::ConditionalNode*>(expr);
            n += countExpr(c->cond.get()) + countExpr(c->thenExpr.get()) + countExpr(c->elseExpr.get());
            break;
        }
        case nyble::ASTType::ArrowFunc: {
            auto* a = static_cast<const nyble::ArrowFuncNode*>(expr);
            n += countStmt(a->body.get()) + countExpr(a->exprBody.get());
            break;
        }
        case nyble::ASTType::New: {
            auto* n2 = static_cast<const nyble::NewExprNode*>(expr);
            n += countExpr(n2->callee.get());
            for (auto& a : n2->args) n += countExpr(a.get());
            break;
        }
        case nyble::ASTType::Identifier:
        case nyble::ASTType::Literal:
            break;
        default: break;
    }
    return n;
}

size_t countStmt(const nyble::Stmt* stmt) {
    if (!stmt) return 0;
    size_t n = 1;
    switch (stmt->type) {
        case nyble::ASTType::Block: {
            auto* b = static_cast<const nyble::BlockStmt*>(stmt);
            for (auto& s : b->stmts) n += countStmt(s.get());
            break;
        }
        case nyble::ASTType::ExprStmt: {
            auto* e = static_cast<const nyble::ExprStmtNode*>(stmt);
            n += countExpr(e->expr.get());
            break;
        }
        case nyble::ASTType::VarDecl: {
            auto* v = static_cast<const nyble::VarDeclNode*>(stmt);
            n += countExpr(v->initializer.get());
            break;
        }
        case nyble::ASTType::FunDecl: {
            auto* f = static_cast<const nyble::FunDeclNode*>(stmt);
            n += countStmt(f->body.get());
            break;
        }
        case nyble::ASTType::If: {
            auto* i = static_cast<const nyble::IfNode*>(stmt);
            n += countExpr(i->cond.get()) + countStmt(i->thenBranch.get()) + countStmt(i->elseBranch.get());
            break;
        }
        case nyble::ASTType::While: {
            auto* w = static_cast<const nyble::WhileNode*>(stmt);
            n += countExpr(w->cond.get()) + countStmt(w->body.get());
            break;
        }
        case nyble::ASTType::DoWhile: {
            auto* d = static_cast<const nyble::DoWhileNode*>(stmt);
            n += countStmt(d->body.get()) + countExpr(d->cond.get());
            break;
        }
        case nyble::ASTType::For: {
            auto* f = static_cast<const nyble::ForNode*>(stmt);
            n += countStmt(f->init.get()) + countExpr(f->cond.get()) + countExpr(f->inc.get()) + countStmt(f->body.get());
            break;
        }
        case nyble::ASTType::Return: {
            auto* r = static_cast<const nyble::ReturnNode*>(stmt);
            n += countExpr(r->value.get());
            break;
        }
        case nyble::ASTType::Switch: {
            auto* s = static_cast<const nyble::SwitchNode*>(stmt);
            n += countExpr(s->expr.get());
            for (auto& [c, stmts] : s->cases) {
                n += countExpr(c.get());
                for (auto& ss : stmts) n += countStmt(ss.get());
            }
            for (auto& ss : s->defaultCase) n += countStmt(ss.get());
            break;
        }
        case nyble::ASTType::Throw: {
            auto* t = static_cast<const nyble::ThrowNode*>(stmt);
            n += countExpr(t->value.get());
            break;
        }
        case nyble::ASTType::Try: {
            auto* t = static_cast<const nyble::TryNode*>(stmt);
            n += countStmt(t->tryBlock.get()) + countStmt(t->catchBlock.get()) + countStmt(t->finallyBlock.get());
            break;
        }
        case nyble::ASTType::Break:
        case nyble::ASTType::Continue:
            break;
        default: break;
    }
    return n;
}

size_t countAST(const nyble::Program& prog) {
    size_t n = 0;
    for (auto& s : prog.stmts) n += countStmt(s.get());
    return n;
}

} // anonymous namespace

static size_t detectSystemMemoryMB() {
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

    size_t sysMemMB = detectSystemMemoryMB();
    if (opts.memSet) {
        nyble::gHeap.setMemoryBudget(opts.memBytes);
    } else {
        nyble::gHeap.setMemoryBudget((sysMemMB * 1024ULL * 1024ULL) / 8ULL);
    }

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

        nyble::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        nyble::Parser parser(tokens);
        auto program = parser.parse();

        if (!parser.getErrors().empty()) {
            for (const auto& err : parser.getErrors()) {
                std::cerr << C_RED << err << C_RESET << "\n";
            }
            return 1;
        }

        try {
            size_t complexity = countAST(program);

            auto runTreeWalk = [&](nyble::Program& prog) {
                nyble::Interpreter interp;
                nyble::gHeap.rootTracer = [&interp](std::vector<nyble::GCHeader*>& wl) {
                    if (interp.currentEnv) interp.currentEnv->traceGCValues(wl);
                };
                interp.evaluate(prog);
            };

            auto runVM = [&](nyble::Program& prog) {
                nyble::BytecodeChunk chunk;
                nyble::Compiler comp(&chunk);
                comp.compile(prog);
                nyble::VM vm;
                nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
                    if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
                };
                vm.run(&chunk, vm.globalEnv);
            };

            if (opts.engine == EngineMode::TreeWalk) {
                runTreeWalk(program);
            } else if (opts.engine == EngineMode::BytecodeVM) {
                runVM(program);
            } else {
                if (complexity < NYBLE_VM_THRESHOLD) {
                    runTreeWalk(program);
                } else {
                    runVM(program);
                }
            }
        } catch (const nyble::Interpreter::ThrowSignal& ts) {
            std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << ts.value.toString() << C_RESET << "\n";
            return 1;
        } catch (const nyble::VMThrow& vt) {
            std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << vt.value.toString() << C_RESET << "\n";
            return 1;
        } catch (const nyble::NybleRuntimeError& e) {
            std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << e.error.toString() << C_RESET << "\n";
            return 1;
        } catch (const std::exception& e) {
            std::cerr << C_RED << C_BOLD << "Internal Error: " << C_RESET << C_RED << e.what() << C_RESET << "\n";
            return 1;
        }

        return 0;
    }

    // REPL
    nyble::VM vm;
    nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
        if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
    };

    std::cout << C_ORANGE << "Nyble" << C_YELLOW << "JS " << C_RESET << C_WHITE << "v" << NYBLE_VERSION << C_RESET << " " << C_GRAY << "(" << NYBLE_VERSION_NAME << ")" << C_RESET << " - JavaScript Engine\n";
    std::cout << C_DIM << "Type 'exit' to quit" << C_RESET << "\n\n";

    enableRawMode();

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

    while (true) {
        std::string lineBuffer;
        size_t cursorPos = 0;
        std::string prompt = source.empty() ? "> " : "  ";

        std::cout << prompt << std::flush;

        while (true) {
            int ch = readKey();

            if (ch == '\r' || ch == '\n') {
                std::cout << "\n" << std::flush;
                break;
            }

            if (ch == 3) { // Ctrl+C
                std::cout << "^C\n" << std::flush;
                lineBuffer.clear();
                source.clear();
                break;
            }

            if (ch == 4) { // Ctrl+D
                disableRawMode();
                std::cout << "\n" << std::flush;
                return 0;
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
                // no-op for single-line
            } else if (ch == 0x101) { // Down
                // no-op for single-line
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

        if (lineBuffer.empty() && source.empty()) continue;

        source += lineBuffer + "\n";

        nyble::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        nyble::Parser parser(tokens);
        auto program = parser.parse();

        if (!parser.getErrors().empty()) {
            bool incomplete = false;
            for (const auto& err : parser.getErrors()) {
                if (err.find("Expected") != std::string::npos) {
                    incomplete = true;
                }
            }
            if (incomplete && source.find(';') == std::string::npos) {
                continue;
            }
            for (const auto& err : parser.getErrors()) {
                std::cerr << C_RED << err << C_RESET << "\n";
            }
            source.clear();
            continue;
        }

        try {
            nyble::BytecodeChunk chunk;
            nyble::Compiler comp(&chunk);
            comp.compile(program);
            try {
                auto result = vm.run(&chunk, vm.globalEnv);
                if (result.type != nyble::ValueType::Undefined && result.type != nyble::ValueType::Null) {
                    std::cout << result.toString() << "\n";
                }
            } catch (const nyble::VMThrow& vt) {
                std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << vt.value.toString() << C_RESET << "\n";
            }
        } catch (const nyble::NybleRuntimeError& e) {
            std::cerr << C_RED << C_BOLD << "Uncaught " << C_RESET << C_RED << e.error.toString() << C_RESET << "\n";
        } catch (const std::exception& e) {
            std::cerr << C_RED << C_BOLD << "Internal Error: " << C_RESET << C_RED << e.what() << C_RESET << "\n";
        }

        source.clear();
    }

    return 0;
}
