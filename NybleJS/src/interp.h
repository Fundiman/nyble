#pragma once
#include <chrono>
#include <random>
#include "ast.h"
#include "env.h"
#include "builtins.h"

namespace nyble {

class Interpreter {
public:
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;

    struct ReturnSignal { Value value; };
    struct BreakSignal {};
    struct ContinueSignal {};
    struct ThrowSignal { Value value; };

    Interpreter();

    Value callValue(const Value& fn, const std::vector<Value>& args, Value thisArg = Value::makeUndefined());

    Value evaluate(const Program& prog);

    Value execute(Stmt* stmt);

    Value evaluateExpr(Expr* expr);

private:
    std::mt19937 rng{std::random_device{}()};

    Value executeBlock(BlockStmt* block);
    Value executeExpr(ExprStmtNode* stmt);
    Value executeVarDecl(VarDeclNode* decl);
    Value executeFunDecl(FunDeclNode* decl);
    Value executeIf(IfNode* stmt);
    Value executeWhile(WhileNode* stmt);
    Value executeDoWhile(DoWhileNode* stmt);
    Value executeFor(ForNode* stmt);
    Value executeForInOf(ForInOfNode* stmt);
    Value executeReturn(ReturnNode* stmt);
    Value executeSwitch(SwitchNode* stmt);
    Value executeThrow(ThrowNode* stmt);
    Value executeTry(TryNode* stmt);

    Value evalBinary(BinaryExprNode* expr);
    Value evalUnary(UnaryExprNode* expr);
    Value evalCall(CallExprNode* expr);
    Value evalMember(MemberExprNode* expr);
    Value evalIdentifier(IdentifierNode* expr);
    Value evalArray(ArrayLitNode* expr);
    Value evalObject(ObjectLitNode* expr);
    Value evalAssign(AssignNode* expr);
    Value evalConditional(ConditionalNode* expr);
    Value evalArrow(ArrowFuncNode* expr);
    Value evalNew(NewExprNode* expr);
};

}
