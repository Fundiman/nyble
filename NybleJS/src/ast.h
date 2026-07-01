#pragma once
#include <string>
#include <vector>
#include <memory>
#include "value.h"

namespace nyble {

enum class ASTType {
    // Statements
    Block, ExprStmt, VarDecl, FunDecl, If, While, DoWhile, For,
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
}
