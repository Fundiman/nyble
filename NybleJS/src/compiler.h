#pragma once
#include "bytecode.h"
#include "ast.h"

namespace nyble {

struct Compiler {
    BytecodeChunk* chunk;
    int scopeDepth;
    size_t loopStart;
    bool hasLoop;

    Compiler(BytecodeChunk* c) : chunk(c), scopeDepth(0), loopStart(0), hasLoop(false) {}

    void compile(Program& prog) {
        for (auto& stmt : prog.stmts) compileStmt(stmt.get());
        emit(OpCode::HALT);
    }

    void emit(OpCode op) { chunk->emit(op, 0); }
    void emitByte(uint8_t b) { chunk->emitByte(b, 0); }
    void emitShort(uint16_t s) { chunk->emitShort(s, 0); }
    size_t emitJump(OpCode op) { emit(op); emitByte(0); emitByte(0); return chunk->count() - 2; }
    void patchJump(size_t offset) { chunk->patch(offset, (uint16_t)(chunk->count() - offset - 2)); }
    void emitLoop(size_t start) { emit(OpCode::LOOP); emitShort((uint16_t)(chunk->count() - start + 2)); }

    size_t makeString(const std::string& s) { return chunk->addStr(s); }
    size_t makeNum(double n) { return chunk->addNum(n); }

    void enterScope() { scopeDepth++; emit(OpCode::SCOPE_ENTER); }
    void exitScope() { scopeDepth--; emit(OpCode::SCOPE_EXIT); }

    void compileStmt(Stmt* stmt) {
        if (!stmt) return;
        switch (stmt->type) {
            case ASTType::Block: compileBlock(static_cast<BlockStmt*>(stmt)); break;
            case ASTType::ExprStmt: compileExprStmt(static_cast<ExprStmtNode*>(stmt)); break;
            case ASTType::VarDecl: compileVarDecl(static_cast<VarDeclNode*>(stmt)); break;
            case ASTType::FunDecl: compileFunDecl(static_cast<FunDeclNode*>(stmt)); break;
            case ASTType::If: compileIf(static_cast<IfNode*>(stmt)); break;
            case ASTType::While: compileWhile(static_cast<WhileNode*>(stmt)); break;
            case ASTType::DoWhile: compileDoWhile(static_cast<DoWhileNode*>(stmt)); break;
            case ASTType::For: compileFor(static_cast<ForNode*>(stmt)); break;
            case ASTType::Return: compileReturn(static_cast<ReturnNode*>(stmt)); break;
            case ASTType::Break: emitJump(OpCode::JMP); break;
            case ASTType::Continue: emitLoop(loopStart); break;
            case ASTType::Switch: compileSwitch(static_cast<SwitchNode*>(stmt)); break;
            case ASTType::Throw: compileThrow(static_cast<ThrowNode*>(stmt)); break;
            case ASTType::Try: compileTry(static_cast<TryNode*>(stmt)); break;
            default: break;
        }
    }

    void compileBlock(BlockStmt* block) {
        enterScope();
        for (auto& s : block->stmts) compileStmt(s.get());
        exitScope();
    }

    void compileExprStmt(ExprStmtNode* stmt) {
        compileExpr(stmt->expr.get());
        emit(OpCode::POP);
    }

    void compileVarDecl(VarDeclNode* decl) {
        if (decl->initializer) compileExpr(decl->initializer.get());
        else emit(OpCode::PUSH_UNDEFINED);
        size_t s = makeString(decl->name);
        emit(OpCode::STORE);
        emitShort((uint16_t)s);
    }

    void compileFunDecl(FunDeclNode* decl) {
        auto funcChunk = new BytecodeChunk();
        funcChunk->params = decl->params;
        Compiler fc(funcChunk);
        fc.scopeDepth = 1;
        fc.enterScope();
        fc.compileBlock(decl->body.get());
        fc.emit(OpCode::PUSH_UNDEFINED);
        fc.emit(OpCode::RETURN);

        size_t idx = chunk->addFunc(funcChunk);
        emit(OpCode::MAKE_FUNCTION);
        emitShort((uint16_t)idx);

        size_t ns = makeString(decl->name);
        emit(OpCode::STORE);
        emitShort((uint16_t)ns);
    }

    void compileIf(IfNode* stmt) {
        compileExpr(stmt->cond.get());
        size_t elseJump = emitJump(OpCode::JMP_IF_FALSE);
        compileStmt(stmt->thenBranch.get());
        size_t endJump = emitJump(OpCode::JMP);
        patchJump(elseJump);
        if (stmt->elseBranch) compileStmt(stmt->elseBranch.get());
        patchJump(endJump);
    }

    void compileWhile(WhileNode* stmt) {
        size_t old = loopStart; bool oldH = hasLoop;
        loopStart = chunk->count(); hasLoop = true;
        compileExpr(stmt->cond.get());
        size_t exit = emitJump(OpCode::JMP_IF_FALSE);
        compileStmt(stmt->body.get());
        emitLoop(loopStart);
        patchJump(exit);
        loopStart = old; hasLoop = oldH;
    }

    void compileDoWhile(DoWhileNode* stmt) {
        size_t old = loopStart; bool oldH = hasLoop;
        loopStart = chunk->count(); hasLoop = true;
        compileStmt(stmt->body.get());
        compileExpr(stmt->cond.get());
        emit(OpCode::JMP_IF_TRUE);
        emitShort((uint16_t)(loopStart - chunk->count() - 2));
        loopStart = old; hasLoop = oldH;
    }

    void compileFor(ForNode* stmt) {
        enterScope();
        if (stmt->init) compileStmt(stmt->init.get());
        size_t old = loopStart; bool oldH = hasLoop;
        loopStart = chunk->count(); hasLoop = true;
        size_t exit = 0;
        if (stmt->cond) { compileExpr(stmt->cond.get()); exit = emitJump(OpCode::JMP_IF_FALSE); }
        compileStmt(stmt->body.get());
        if (stmt->inc) { compileExpr(stmt->inc.get()); emit(OpCode::POP); }
        emitLoop(loopStart);
        if (stmt->cond) patchJump(exit);
        loopStart = old; hasLoop = oldH;
        exitScope();
    }

    void compileReturn(ReturnNode* stmt) {
        if (stmt->value) compileExpr(stmt->value.get());
        else emit(OpCode::PUSH_UNDEFINED);
        emit(OpCode::RETURN);
    }

    void compileSwitch(SwitchNode* stmt) {
        compileExpr(stmt->expr.get());
        emit(OpCode::DUP);
        std::vector<size_t> jumps;
        size_t defPatch = 0; bool hasDef = false;

        for (auto& ce : stmt->cases) {
            compileExpr(ce.first.get());
            emit(OpCode::STRICT_EQ);
            size_t j = emitJump(OpCode::JMP_IF_FALSE);
            emit(OpCode::POP);
            enterScope();
            for (auto& s : ce.second) compileStmt(s.get());
            exitScope();
            jumps.push_back(emitJump(OpCode::JMP));
            patchJump(j);
        }

        if (!stmt->defaultCase.empty()) {
            hasDef = true;
            emit(OpCode::POP);
            enterScope();
            for (auto& s : stmt->defaultCase) compileStmt(s.get());
            exitScope();
            defPatch = emitJump(OpCode::JMP);
        }

        for (auto j : jumps) patchJump(j);
        if (hasDef) patchJump(defPatch);
        emit(OpCode::POP);
    }

    void compileThrow(ThrowNode* stmt) {
        if (stmt->value) compileExpr(stmt->value.get());
        else emit(OpCode::PUSH_UNDEFINED);
        emit(OpCode::THROW);
    }

    void compileTry(TryNode* stmt) {
        // PUSH_TRY catch_offset finally_offset (absolute byte offsets from chunk start)
        size_t tryInstr = chunk->count();
        emit(OpCode::PUSH_TRY);
        emitByte(0); emitByte(0); // catch offset placeholder (absolute)
        emitByte(0); emitByte(0); // finally offset placeholder (absolute)

        // Try block
        enterScope();
        for (auto& s : stmt->tryBlock->stmts) compileStmt(s.get());
        exitScope();
        emit(OpCode::POP_TRY);

        // Normal path: compile finally inline (if present)
        if (stmt->finallyBlock) {
            enterScope();
            for (auto& s : stmt->finallyBlock->stmts) compileStmt(s.get());
            exitScope();
        }
        size_t afterTry = emitJump(OpCode::JMP);

        // Catch/Finally exception handler area starts here
        size_t handlerPos = chunk->count();

        if (stmt->catchBlock) {
            // Patch catch offset (absolute position)
            chunk->patch(tryInstr + 1, (uint16_t)handlerPos);

            // Exception enters catch handler - thrown value is on stack
            // (throw handler already popped the PUSH_TRY entry)
            size_t ns = makeString(stmt->catchParam);
            emit(OpCode::STORE);
            emitShort((uint16_t)ns);

            size_t innerTryPos = 0;
            if (stmt->finallyBlock) {
                // Wrap catch body in inner try/finally so finally runs if catch throws
                innerTryPos = chunk->count();
                emit(OpCode::PUSH_TRY);
                emitByte(0); emitByte(0); // no catch (absolute)
                emitByte(0); emitByte(0); // finally placeholder (absolute)
            }

            enterScope();
            for (auto& s : stmt->catchBlock->stmts) compileStmt(s.get());
            exitScope();

            if (stmt->finallyBlock) {
                emit(OpCode::POP_TRY);
                // Normal exit from catch: compile finally inline
                enterScope();
                for (auto& s : stmt->finallyBlock->stmts) compileStmt(s.get());
                exitScope();
                size_t afterCatch = emitJump(OpCode::JMP);

                // Exception path from catch body: inner finally handler
                size_t innerFinallyPos = chunk->count();
                chunk->patch(innerTryPos + 3, (uint16_t)innerFinallyPos);

                // Duplicate finally block for exception path
                enterScope();
                for (auto& s : stmt->finallyBlock->stmts) compileStmt(s.get());
                exitScope();
                emit(OpCode::RETHROW);

                patchJump(afterCatch);
            }
        }

        if (stmt->finallyBlock && !stmt->catchBlock) {
            // Only finally, no catch - exception enters directly at handlerPos
            chunk->patch(tryInstr + 3, (uint16_t)handlerPos);

            // No catch entry - just emit duplicated finally for exception path
            // (isThrowing stays true after finally, will propagate)
            enterScope();
            for (auto& s : stmt->finallyBlock->stmts) compileStmt(s.get());
            exitScope();
            // isThrowing is still true, so at loop top it'll propagate
        }

        patchJump(afterTry);
    }

    void compileExpr(Expr* expr) {
        if (!expr) { emit(OpCode::PUSH_UNDEFINED); return; }
        switch (expr->type) {
            case ASTType::Binary: compileBinary(static_cast<BinaryExprNode*>(expr)); break;
            case ASTType::Unary: compileUnary(static_cast<UnaryExprNode*>(expr)); break;
            case ASTType::Call: compileCall(static_cast<CallExprNode*>(expr)); break;
            case ASTType::Member: compileMember(static_cast<MemberExprNode*>(expr)); break;
            case ASTType::Identifier: compileIdentifier(static_cast<IdentifierNode*>(expr)); break;
            case ASTType::Literal: compileLiteral(static_cast<LiteralNode*>(expr)); break;
            case ASTType::ArrayLit: compileArray(static_cast<ArrayLitNode*>(expr)); break;
            case ASTType::ObjectLit: compileObject(static_cast<ObjectLitNode*>(expr)); break;
            case ASTType::Assignment: compileAssign(static_cast<AssignNode*>(expr)); break;
            case ASTType::Conditional: compileConditional(static_cast<ConditionalNode*>(expr)); break;
            case ASTType::ArrowFunc: compileArrow(static_cast<ArrowFuncNode*>(expr)); break;
            case ASTType::New: compileNew(static_cast<NewExprNode*>(expr)); break;
            default: emit(OpCode::PUSH_UNDEFINED);
        }
    }

    void compileBinary(BinaryExprNode* expr) {
        if (expr->op == "&&") {
            compileExpr(expr->left.get());
            size_t j = emitJump(OpCode::JMP_IF_FALSE);
            emit(OpCode::POP);
            compileExpr(expr->right.get());
            patchJump(j);
            return;
        }
        if (expr->op == "||") {
            compileExpr(expr->left.get());
            size_t j = emitJump(OpCode::JMP_IF_TRUE);
            emit(OpCode::POP);
            compileExpr(expr->right.get());
            patchJump(j);
            return;
        }
        compileExpr(expr->left.get());
        compileExpr(expr->right.get());
        #define BINOP(O, I) if (expr->op == O) { emit(I); return; }
        BINOP("+", OpCode::ADD) BINOP("-", OpCode::SUB) BINOP("*", OpCode::MUL)
        BINOP("/", OpCode::DIV) BINOP("%", OpCode::MOD) BINOP("**", OpCode::POW)
        BINOP("==", OpCode::EQ) BINOP("!=", OpCode::NEQ) BINOP("===", OpCode::STRICT_EQ)
        BINOP("!==", OpCode::STRICT_NEQ) BINOP("<", OpCode::LT) BINOP(">", OpCode::GT)
        BINOP("<=", OpCode::LTE) BINOP(">=", OpCode::GTE)
        #undef BINOP
    }

    void compileUnary(UnaryExprNode* expr) {
        if ((expr->op == "++" || expr->op == "--") && expr->operand->type == ASTType::Identifier) {
            auto id = static_cast<IdentifierNode*>(expr->operand.get());
            size_t ns = makeString(id->name);
            bool pre = expr->prefix;
            OpCode loadOp = expr->op == "++" ? OpCode::ADD : OpCode::SUB;

            emit(OpCode::LOAD);
            emitShort((uint16_t)ns);
            if (!pre) emit(OpCode::DUP);
            size_t one = makeNum(1.0);
            emit(OpCode::PUSH_NUM);
            emitShort((uint16_t)one);
            emit(loadOp);
            if (pre) emit(OpCode::DUP);
            emit(OpCode::STORE);
            emitShort((uint16_t)ns);
            return;
        }
        compileExpr(expr->operand.get());
        if (expr->op == "-") emit(OpCode::NEGATE);
        else if (expr->op == "!") emit(OpCode::NOT);
        else if (expr->op == "typeof") emit(OpCode::TYPEOF);
    }

    void compileCall(CallExprNode* expr) {
        if (expr->callee->type == ASTType::Member) {
            auto mem = static_cast<MemberExprNode*>(expr->callee.get());
            compileExpr(mem->object.get());
            emit(OpCode::DUP);
            if (!mem->computed && mem->property->type == ASTType::Identifier) {
                auto id = static_cast<IdentifierNode*>(mem->property.get());
                size_t s = makeString(id->name);
                emit(OpCode::GET_PROP);
                emitShort((uint16_t)s);
            } else {
                compileExpr(mem->property.get());
                emit(OpCode::GET_INDEX);
            }
            for (auto& a : expr->args) compileExpr(a.get());
            emit(OpCode::CALL_METHOD);
            emitShort((uint16_t)expr->args.size());
        } else {
            compileExpr(expr->callee.get());
            emit(OpCode::PUSH_UNDEFINED);
            for (auto& a : expr->args) compileExpr(a.get());
            emit(OpCode::CALL);
            emitShort((uint16_t)expr->args.size());
        }
    }

    void compileMember(MemberExprNode* expr) {
        compileExpr(expr->object.get());
        if (!expr->computed && expr->property->type == ASTType::Identifier) {
            auto id = static_cast<IdentifierNode*>(expr->property.get());
            size_t s = makeString(id->name);
            emit(OpCode::GET_PROP);
            emitShort((uint16_t)s);
        } else {
            compileExpr(expr->property.get());
            emit(OpCode::GET_INDEX);
        }
    }

    void compileIdentifier(IdentifierNode* expr) {
        size_t s = makeString(expr->name);
        emit(OpCode::LOAD);
        emitShort((uint16_t)s);
    }

    void compileLiteral(LiteralNode* expr) {
        auto& v = expr->value;
        switch (v.type) {
            case ValueType::Null: emit(OpCode::PUSH_NULL); break;
            case ValueType::Undefined: emit(OpCode::PUSH_UNDEFINED); break;
            case ValueType::Boolean: emit(v.boolVal ? OpCode::PUSH_TRUE : OpCode::PUSH_FALSE); break;
            case ValueType::Number: emit(OpCode::PUSH_NUM); emitShort((uint16_t)makeNum(v.numVal)); break;
            case ValueType::String: emit(OpCode::PUSH_STRING); emitShort((uint16_t)makeString(v.strVal ? v.strVal->str : "")); break;
            default: emit(OpCode::PUSH_UNDEFINED);
        }
    }

    void compileArray(ArrayLitNode* expr) {
        for (auto& e : expr->elements) {
            if (e) compileExpr(e.get());
            else emit(OpCode::PUSH_UNDEFINED);
        }
        emit(OpCode::NEW_ARRAY);
        emitShort((uint16_t)expr->elements.size());
    }

    void compileObject(ObjectLitNode* expr) {
        emit(OpCode::NEW_OBJECT);
        for (auto& [key, valExpr] : expr->properties) {
            compileExpr(valExpr.get());
            size_t s = makeString(key);
            emit(OpCode::SET_PROP);
            emitShort((uint16_t)s);
            emit(OpCode::POP);
        }
    }

    void compileAssign(AssignNode* expr) {
        if (expr->target->type == ASTType::Identifier) {
            auto id = static_cast<IdentifierNode*>(expr->target.get());
            size_t ns = makeString(id->name);

            if (expr->op == "=") {
                compileExpr(expr->value.get());
                emit(OpCode::DUP);
                emit(OpCode::STORE);
                emitShort((uint16_t)ns);
            } else {
                emit(OpCode::LOAD);
                emitShort((uint16_t)ns);
                compileExpr(expr->value.get());
                if (expr->op == "+=") emit(OpCode::ADD);
                else if (expr->op == "-=") emit(OpCode::SUB);
                else if (expr->op == "*=") emit(OpCode::MUL);
                else if (expr->op == "/=") emit(OpCode::DIV);
                else if (expr->op == "%=") emit(OpCode::MOD);
                emit(OpCode::DUP);
                emit(OpCode::STORE);
                emitShort((uint16_t)ns);
            }
        } else if (expr->target->type == ASTType::Member) {
            auto mem = static_cast<MemberExprNode*>(expr->target.get());
            compileExpr(mem->object.get());
            compileExpr(expr->value.get());
            if (!mem->computed && mem->property->type == ASTType::Identifier) {
                auto id = static_cast<IdentifierNode*>(mem->property.get());
                size_t s = makeString(id->name);
                if (expr->op == "=") {
                    emit(OpCode::DUP);
                    emit(OpCode::SET_PROP);
                    emitShort((uint16_t)s);
                }
            }
        }
    }

    void compileConditional(ConditionalNode* expr) {
        compileExpr(expr->cond.get());
        size_t elseJ = emitJump(OpCode::JMP_IF_FALSE);
        emit(OpCode::POP);
        compileExpr(expr->thenExpr.get());
        size_t endJ = emitJump(OpCode::JMP);
        patchJump(elseJ);
        emit(OpCode::POP);
        compileExpr(expr->elseExpr.get());
        patchJump(endJ);
    }

    void compileNew(NewExprNode* expr) {
        compileExpr(expr->callee.get());
        for (auto& a : expr->args) compileExpr(a.get());
        emit(OpCode::NEW);
        emitShort((uint16_t)expr->args.size());
    }

    void compileArrow(ArrowFuncNode* expr) {
        auto funcChunk = new BytecodeChunk();
        funcChunk->params = expr->params;
        Compiler fc(funcChunk);
        fc.scopeDepth = 1;
        fc.enterScope();
        if (expr->isExprBody && expr->exprBody) fc.compileExpr(expr->exprBody.get());
        else { fc.compileStmt(expr->body.get()); fc.emit(OpCode::PUSH_UNDEFINED); }
        fc.emit(OpCode::RETURN);

        size_t idx = chunk->addFunc(funcChunk);
        emit(OpCode::MAKE_ARROW_FUNCTION);
        emitShort((uint16_t)idx);
    }
};

}
