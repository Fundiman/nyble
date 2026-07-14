#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "nyblejs.h"

#ifndef NYBLE_VM_THRESHOLD
#define NYBLE_VM_THRESHOLD 300
#endif

static const char* NYBLE_VERSION = "0.2.0";

enum class EngineMode { Auto, TreeWalk, BytecodeVM, JIT };

static std::string uwuify(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == 'r') c = 'w';
        else if (c == 'R') c = 'W';
        else if (c == 'l') c = 'w';
        else if (c == 'L') c = 'W';
    }
    return out;
}

static void printMeow() {
    seed_once();
    bool isFact = rand() % 2;
    const char* author;
    const char* text;
    if (isFact) {
        Fact f = get_fact();
        author = f.author;
        text = f.text;
    } else {
        Quote q = get_quote();
        author = q.author;
        text = q.text;
    }
    std::string uwuText = uwuify(text);
    std::string uwuAuthor = "— " + uwuify(author);
    std::cout << "\n"
              << "  /\\_/\\  \n"
              << " ( o.o ) \n"
              << "  > ^ <  \n"
              << " /|   |\\ \n"
              << "(_|   |_)\n"
              << "\n"
              << "  " << uwuText << "\n"
              << "  " << uwuAuthor << "\n"
              << "\n";
}

static void printHelp() {
    std::cout               << "Usage: njs [options] <file>\n"
              << "       njs [options]          (REPL mode)\n"
              << "\n"
              << "Options:\n"
              << "  -help, --help          Show this help message\n"
              << "  -version, --version    Show version info\n"
              << "  -Xmx<size>             Set GC max memory budget (e.g. 256m, 1g)\n"
              << "  -engine, --engine <type>  Force execution engine:\n"
              << "                            tree    - Tree-walking interpreter only\n"
              << "                            vm      - Bytecode VM only\n"
              << "                            jit     - JIT compiler (not yet implemented)\n"
              << "  -c <code>              Execute inline JavaScript code\n"
              << "  -D<key>=<value>        Set a custom property (accessible via Nyble.props)\n"
              << "\n"
              << "Examples:\n"
              << "  njs script.js\n"
              << "  njs -engine tree script.js\n"
              << "  njs -Xmx512m -Dname=app script.js\n"
              << "  njs --engine vm -Xmx1g script.js\n"
              << "  njs -c \"console.log(42)\"\n"
              << "  njs --meow\n"
              << "\n"
              << "This NJS has Super Cat Powers.\n";
}

static void printVersion() {
    std::cout << "NybleJS v" << NYBLE_VERSION << " (Hybrid Engine)\n";
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
                std::cerr << "Error: " << arg << " requires a value (tree, vm, jit)\n";
                exit(1);
            }
            std::string val = argv[i + 1];
            if (val == "tree") opts.engine = EngineMode::TreeWalk;
            else if (val == "vm") opts.engine = EngineMode::BytecodeVM;
            else if (val == "jit") opts.engine = EngineMode::JIT;
            else {
                std::cerr << "Error: Unknown engine '" << val << "'. Use tree, vm, or jit.\n";
                exit(1);
            }
            i += 2;
        } else if (arg == "-c") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -c requires a code string\n";
                exit(1);
            }
            opts.code = argv[i + 1];
            i += 2;
        } else if (arg.substr(0, 2) == "-D") {
            std::string prop = arg.substr(2);
            auto eq = prop.find('=');
            if (eq == std::string::npos) {
                std::cerr << "Error: Invalid property format. Use -Dkey=value\n";
                exit(1);
            }
            opts.props[prop.substr(0, eq)] = prop.substr(eq + 1);
            i++;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'. Use -help for usage.\n";
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

int main(int argc, char* argv[]) {
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
        std::cerr << "Error: JIT compiler is not yet implemented.\n";
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
                std::cerr << "Error: Cannot open file '" << opts.filename << "'\n";
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
                std::cerr << err << "\n";
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
            std::cerr << "Uncaught: " << ts.value.toString() << "\n";
            return 1;
        } catch (const nyble::VMThrow& vt) {
            std::cerr << "Uncaught: " << vt.value.toString() << "\n";
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

    // REPL
    nyble::VM vm;
    nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
        if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
    };

    std::cout << "NybleJS v" << NYBLE_VERSION << " (Hybrid Engine) - JavaScript Engine\n";
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
                std::cerr << "Uncaught: " << vt.value.toString() << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }

        source.clear();
    }

    return 0;
}
