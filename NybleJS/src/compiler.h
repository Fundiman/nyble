#pragma once
#include "bytecode.h"
#include "ast.h"

namespace nyble {

struct Compiler {
    BytecodeChunk* chunk;
    int scopeDepth;
    size_t loopStart;
    bool hasLoop;

    Compiler(BytecodeChunk* c);

    void compile(Program& prog);

    void emit(OpCode op);
    void emitByte(uint8_t b);
    void emitShort(uint16_t s);
    size_t emitJump(OpCode op);
    void patchJump(size_t offset);
    void emitLoop(size_t start);

    size_t makeString(const std::string& s);
    size_t makeNum(double n);

    void enterScope();
    void exitScope();

    void compileStmt(Stmt* stmt);
    void compileBlock(BlockStmt* block);
    void compileExprStmt(ExprStmtNode* stmt);
    void compileVarDecl(VarDeclNode* decl);
    void compileFunDecl(FunDeclNode* decl);
    void compileIf(IfNode* stmt);
    void compileWhile(WhileNode* stmt);
    void compileDoWhile(DoWhileNode* stmt);
    void compileFor(ForNode* stmt);
    void compileReturn(ReturnNode* stmt);
    void compileSwitch(SwitchNode* stmt);
    void compileThrow(ThrowNode* stmt);
    void compileTry(TryNode* stmt);

    void compileExpr(Expr* expr);
    void compileBinary(BinaryExprNode* expr);
    void compileUnary(UnaryExprNode* expr);
    void compileCall(CallExprNode* expr);
    void compileMember(MemberExprNode* expr);
    void compileIdentifier(IdentifierNode* expr);
    void compileLiteral(LiteralNode* expr);
    void compileArray(ArrayLitNode* expr);
    void compileObject(ObjectLitNode* expr);
    void compileAssign(AssignNode* expr);
    void compileConditional(ConditionalNode* expr);
    void compileNew(NewExprNode* expr);
    void compileArrow(ArrowFuncNode* expr);
};

}
