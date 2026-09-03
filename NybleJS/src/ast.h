#pragma once
#include <string>
#include <vector>
#include <memory>
#include "value.h"

namespace nyble {

enum class ASTType {
    // Statements
    Block, ExprStmt, VarDecl, FunDecl, If, While, DoWhile, For, ForIn, ForOf,
    Return, Break, Continue, Switch, Throw, Try,
    // Expressions
    Binary, Unary, Call, Member, Identifier, Literal,
    ArrayLit, ObjectLit, Assignment, Conditional, ArrowFunc, New
};

struct ASTNode {
    ASTType type;
    size_t line = 0;
    virtual ~ASTNode() = default;
};

struct Stmt : ASTNode { virtual ~Stmt() = default; };
struct Expr : ASTNode { virtual ~Expr() = default; };

struct BlockStmt : Stmt { BlockStmt() { type = ASTType::Block; } std::vector<std::unique_ptr<Stmt>> stmts; };
struct ExprStmtNode : Stmt { ExprStmtNode() { type = ASTType::ExprStmt; } std::unique_ptr<Expr> expr; };
struct VarDeclNode : Stmt {
    VarDeclNode() { type = ASTType::VarDecl; }
    std::string name;
    bool isConst = false;
    std::unique_ptr<Expr> initializer;
};
struct FunDeclNode : Stmt {
    FunDeclNode() { type = ASTType::FunDecl; }
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<BlockStmt> body;
};
struct IfNode : Stmt {
    IfNode() { type = ASTType::If; }
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;
};
struct WhileNode : Stmt { WhileNode() { type = ASTType::While; } std::unique_ptr<Expr> cond; std::unique_ptr<Stmt> body; };
struct DoWhileNode : Stmt { DoWhileNode() { type = ASTType::DoWhile; } std::unique_ptr<Stmt> body; std::unique_ptr<Expr> cond; };
struct ForNode : Stmt {
    ForNode() { type = ASTType::For; }
    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> inc;
    std::unique_ptr<Stmt> body;
};
struct ForInOfNode : Stmt {
    ForInOfNode() { type = ASTType::ForIn; }
    std::string varName;
    bool isConst = false;
    bool isOf = false;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Stmt> body;
};
struct ReturnNode : Stmt { ReturnNode() { type = ASTType::Return; } std::unique_ptr<Expr> value; };
struct BreakNode : Stmt { BreakNode() { type = ASTType::Break; } };
struct ContinueNode : Stmt { ContinueNode() { type = ASTType::Continue; } };
struct SwitchNode : Stmt {
    SwitchNode() { type = ASTType::Switch; }
    std::unique_ptr<Expr> expr;
    std::vector<std::pair<std::unique_ptr<Expr>, std::vector<std::unique_ptr<Stmt>>>> cases;
    std::vector<std::unique_ptr<Stmt>> defaultCase;
};

struct ThrowNode : Stmt {
    ThrowNode() { type = ASTType::Throw; }
    std::unique_ptr<Expr> value;
};
struct TryNode : Stmt {
    TryNode() { type = ASTType::Try; }
    std::unique_ptr<BlockStmt> tryBlock;
    std::string catchParam;
    std::unique_ptr<BlockStmt> catchBlock;
    std::unique_ptr<BlockStmt> finallyBlock;
};

struct BinaryExprNode : Expr { BinaryExprNode() { type = ASTType::Binary; } std::unique_ptr<Expr> left; std::unique_ptr<Expr> right; std::string op; };
struct UnaryExprNode : Expr { UnaryExprNode() { type = ASTType::Unary; } std::string op; std::unique_ptr<Expr> operand; bool prefix = true; };
struct CallExprNode : Expr { CallExprNode() { type = ASTType::Call; } std::unique_ptr<Expr> callee; std::vector<std::unique_ptr<Expr>> args; };
struct MemberExprNode : Expr { MemberExprNode() { type = ASTType::Member; } std::unique_ptr<Expr> object; std::unique_ptr<Expr> property; bool computed = false; };
struct IdentifierNode : Expr { IdentifierNode() { type = ASTType::Identifier; } std::string name; };
struct LiteralNode : Expr { LiteralNode() { type = ASTType::Literal; } Value value; };
struct ArrayLitNode : Expr { ArrayLitNode() { type = ASTType::ArrayLit; } std::vector<std::unique_ptr<Expr>> elements; };
struct ObjectLitNode : Expr {
    ObjectLitNode() { type = ASTType::ObjectLit; }
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> properties;
};
struct AssignNode : Expr { AssignNode() { type = ASTType::Assignment; } std::unique_ptr<Expr> target; std::unique_ptr<Expr> value; std::string op; };
struct ConditionalNode : Expr { ConditionalNode() { type = ASTType::Conditional; } std::unique_ptr<Expr> cond; std::unique_ptr<Expr> thenExpr; std::unique_ptr<Expr> elseExpr; };
struct ArrowFuncNode : Expr {
    ArrowFuncNode() { type = ASTType::ArrowFunc; }
    std::vector<std::string> params;
    std::unique_ptr<Stmt> body;
    bool isExprBody = false;
    std::unique_ptr<Expr> exprBody;
    bool isFuncExpr = false;
};

struct NewExprNode : Expr {
    NewExprNode() { type = ASTType::New; }
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct Program {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

// AST complexity estimation, used to auto-select between the tree-walking
// interpreter and the bytecode VM (see Context).
inline size_t countStmt(const Stmt* stmt);
inline size_t countExpr(const Expr* expr);

inline size_t countExpr(const Expr* expr) {
    if (!expr) return 0;
    size_t n = 1;
    switch (expr->type) {
        case ASTType::Binary: {
            auto* b = static_cast<const BinaryExprNode*>(expr);
            n += countExpr(b->left.get()) + countExpr(b->right.get());
            break;
        }
        case ASTType::Unary: {
            auto* u = static_cast<const UnaryExprNode*>(expr);
            n += countExpr(u->operand.get());
            break;
        }
        case ASTType::Call: {
            auto* c = static_cast<const CallExprNode*>(expr);
            n += countExpr(c->callee.get());
            for (auto& a : c->args) n += countExpr(a.get());
            break;
        }
        case ASTType::Member: {
            auto* m = static_cast<const MemberExprNode*>(expr);
            n += countExpr(m->object.get()) + countExpr(m->property.get());
            break;
        }
        case ASTType::ArrayLit: {
            auto* a = static_cast<const ArrayLitNode*>(expr);
            for (auto& e : a->elements) n += countExpr(e.get());
            break;
        }
        case ASTType::ObjectLit: {
            auto* o = static_cast<const ObjectLitNode*>(expr);
            for (auto& [k, v] : o->properties) n += countExpr(v.get());
            break;
        }
        case ASTType::Assignment: {
            auto* a = static_cast<const AssignNode*>(expr);
            n += countExpr(a->target.get()) + countExpr(a->value.get());
            break;
        }
        case ASTType::Conditional: {
            auto* c = static_cast<const ConditionalNode*>(expr);
            n += countExpr(c->cond.get()) + countExpr(c->thenExpr.get()) + countExpr(c->elseExpr.get());
            break;
        }
        case ASTType::ArrowFunc: {
            auto* a = static_cast<const ArrowFuncNode*>(expr);
            n += countStmt(a->body.get()) + countExpr(a->exprBody.get());
            break;
        }
        case ASTType::New: {
            auto* n2 = static_cast<const NewExprNode*>(expr);
            n += countExpr(n2->callee.get());
            for (auto& a : n2->args) n += countExpr(a.get());
            break;
        }
        case ASTType::Identifier:
        case ASTType::Literal:
            break;
        default: break;
    }
    return n;
}

inline size_t countStmt(const Stmt* stmt) {
    if (!stmt) return 0;
    size_t n = 1;
    switch (stmt->type) {
        case ASTType::Block: {
            auto* b = static_cast<const BlockStmt*>(stmt);
            for (auto& s : b->stmts) n += countStmt(s.get());
            break;
        }
        case ASTType::ExprStmt: {
            auto* e = static_cast<const ExprStmtNode*>(stmt);
            n += countExpr(e->expr.get());
            break;
        }
        case ASTType::VarDecl: {
            auto* v = static_cast<const VarDeclNode*>(stmt);
            n += countExpr(v->initializer.get());
            break;
        }
        case ASTType::FunDecl: {
            auto* f = static_cast<const FunDeclNode*>(stmt);
            n += countStmt(f->body.get());
            break;
        }
        case ASTType::If: {
            auto* i = static_cast<const IfNode*>(stmt);
            n += countExpr(i->cond.get()) + countStmt(i->thenBranch.get()) + countStmt(i->elseBranch.get());
            break;
        }
        case ASTType::While: {
            auto* w = static_cast<const WhileNode*>(stmt);
            n += countExpr(w->cond.get()) + countStmt(w->body.get());
            break;
        }
        case ASTType::DoWhile: {
            auto* d = static_cast<const DoWhileNode*>(stmt);
            n += countStmt(d->body.get()) + countExpr(d->cond.get());
            break;
        }
        case ASTType::For: {
            auto* f = static_cast<const ForNode*>(stmt);
            n += countStmt(f->init.get()) + countExpr(f->cond.get()) + countExpr(f->inc.get()) + countStmt(f->body.get());
            break;
        }
        case ASTType::Return: {
            auto* r = static_cast<const ReturnNode*>(stmt);
            n += countExpr(r->value.get());
            break;
        }
        case ASTType::Switch: {
            auto* s = static_cast<const SwitchNode*>(stmt);
            n += countExpr(s->expr.get());
            for (auto& [c, stmts] : s->cases) {
                n += countExpr(c.get());
                for (auto& ss : stmts) n += countStmt(ss.get());
            }
            for (auto& ss : s->defaultCase) n += countStmt(ss.get());
            break;
        }
        case ASTType::Throw: {
            auto* t = static_cast<const ThrowNode*>(stmt);
            n += countExpr(t->value.get());
            break;
        }
        case ASTType::Try: {
            auto* t = static_cast<const TryNode*>(stmt);
            n += countStmt(t->tryBlock.get()) + countStmt(t->catchBlock.get()) + countStmt(t->finallyBlock.get());
            break;
        }
        case ASTType::Break:
        case ASTType::Continue:
        case ASTType::ForIn:
        case ASTType::ForOf:
            break;
        default: break;
    }
    return n;
}

inline size_t countAST(const Program& prog) {
    size_t n = 0;
    for (auto& s : prog.stmts) n += countStmt(s.get());
    return n;
}

}
