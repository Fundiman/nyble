#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "lexer.h"
#include "parser.h"
#include "interp.h"
#include "compiler.h"
#include "vm.h"

#ifndef NYBLE_VM_THRESHOLD
#define NYBLE_VM_THRESHOLD 300
#endif

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
    size_t sysMemMB = detectSystemMemoryMB();
    nyble::gHeap.setMemoryBudget((sysMemMB * 1024ULL * 1024ULL) / 8ULL);

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
            size_t complexity = countAST(program);

            if (complexity < NYBLE_VM_THRESHOLD) {
                nyble::Interpreter interp;
                nyble::gHeap.rootTracer = [&interp](std::vector<nyble::GCHeader*>& wl) {
                    if (interp.currentEnv) interp.currentEnv->traceGCValues(wl);
                };
                interp.evaluate(program);
            } else {
                nyble::BytecodeChunk chunk;
                nyble::Compiler comp(&chunk);
                comp.compile(program);
                nyble::VM vm;
                nyble::gHeap.rootTracer = [&vm](std::vector<nyble::GCHeader*>& wl) {
                    if (vm.globalEnv) vm.globalEnv->traceGCValues(wl);
                };
                try {
                    vm.run(&chunk, vm.globalEnv);
                } catch (const nyble::VMThrow& vt) {
                    std::cerr << "Uncaught: " << vt.value.toString() << "\n";
                    return 1;
                }
            }
        } catch (const nyble::Interpreter::ThrowSignal& ts) {
            std::cerr << "Uncaught: " << ts.value.toString() << "\n";
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
    std::cout << "NybleJS v0.2.0 (Hybrid Engine) - JavaScript Engine\n";
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
