#pragma once
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include "ast.h"

namespace nyble {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens(std::move(tokens)), pos(0) {}

    Program parse() {
        Program prog;
        while (!isAtEnd()) {
            auto stmt = parseStmt();
            if (stmt) prog.stmts.push_back(std::move(stmt));
        }
        return prog;
    }

    std::vector<std::string> getErrors() const { return errors; }

private:
    std::vector<Token> tokens;
    size_t pos;
    std::vector<std::string> errors;

    const Token& peek() const { return tokens[pos]; }
    const Token& previous() const { return tokens[pos - 1]; }
    bool isAtEnd() const { return peek().type == TokenType::Eof; }

    Token advance() {
        if (!isAtEnd()) pos++;
        return previous();
    }

    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check(type)) { advance(); return true; }
        return false;
    }

    bool matchAny(std::initializer_list<TokenType> types) {
        for (auto t : types) {
            if (match(t)) return true;
        }
        return false;
    }

    Token consume(TokenType type, const std::string& msg) {
        if (check(type)) return advance();
        error(msg);
        if (!isAtEnd()) advance();
        return Token();
    }

    void error(const std::string& msg) {
        std::ostringstream os;
        os << "Parse error at " << peek().line << ":" << peek().col << " - " << msg;
        errors.push_back(os.str());
    }

    std::unique_ptr<Stmt> parseStmt() {
        if (match(TokenType::LBrace)) return parseBlock();
        if (matchAny({TokenType::Let, TokenType::Const, TokenType::Var})) return parseVarDecl();
        if (match(TokenType::Function)) return parseFunDecl();
        if (match(TokenType::If)) return parseIfStmt();
        if (match(TokenType::While)) return parseWhileStmt();
        if (match(TokenType::Do)) return parseDoWhileStmt();
        if (match(TokenType::For)) return parseForStmt();
        if (match(TokenType::Return)) return parseReturnStmt();
        if (match(TokenType::Break)) { auto s = std::make_unique<BreakNode>(); s->line = previous().line; consume(TokenType::Semicolon, ";"); return s; }
        if (match(TokenType::Continue)) { auto s = std::make_unique<ContinueNode>(); s->line = previous().line; consume(TokenType::Semicolon, ";"); return s; }
        if (match(TokenType::Switch)) return parseSwitchStmt();
        if (match(TokenType::Throw)) return parseThrowStmt();
        if (match(TokenType::Try)) return parseTryStmt();
        return parseExprStmt();
    }

    std::unique_ptr<BlockStmt> parseBlock() {
        auto block = std::make_unique<BlockStmt>();
        block->line = previous().line;
        while (!isAtEnd() && !check(TokenType::RBrace)) {
            auto s = parseStmt();
            if (s) block->stmts.push_back(std::move(s));
        }
        consume(TokenType::RBrace, "Expected '}'");
        return block;
    }

    std::unique_ptr<VarDeclNode> parseVarDecl() {
        auto decl = std::make_unique<VarDeclNode>();
        TokenType declType = previous().type;
        decl->isConst = (declType == TokenType::Const);
        decl->line = previous().line;
        Token name = consume(TokenType::Identifier, "Expected variable name");
        decl->name = name.lexeme;
        if (match(TokenType::Eq)) {
            decl->initializer = parseExpr();
        }
        consume(TokenType::Semicolon, "Expected ';'");
        return decl;
    }

    std::unique_ptr<FunDeclNode> parseFunDecl() {
        auto decl = std::make_unique<FunDeclNode>();
        decl->line = previous().line;
        Token name = consume(TokenType::Identifier, "Expected function name");
        decl->name = name.lexeme;
        consume(TokenType::LParen, "Expected '('");
        if (!check(TokenType::RParen)) {
            do {
                Token p = consume(TokenType::Identifier, "Expected parameter");
                decl->params.push_back(p.lexeme);
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')'");
        consume(TokenType::LBrace, "Expected '{'");
        decl->body = parseBlock();
        return decl;
    }

    std::unique_ptr<IfNode> parseIfStmt() {
        auto stmt = std::make_unique<IfNode>();
        stmt->line = previous().line;
        consume(TokenType::LParen, "Expected '('");
        stmt->cond = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        stmt->thenBranch = parseStmt();
        if (match(TokenType::Else)) {
            stmt->elseBranch = parseStmt();
        }
        return stmt;
    }

    std::unique_ptr<WhileNode> parseWhileStmt() {
        auto stmt = std::make_unique<WhileNode>();
        stmt->line = previous().line;
        consume(TokenType::LParen, "Expected '('");
        stmt->cond = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        stmt->body = parseStmt();
        return stmt;
    }

    std::unique_ptr<DoWhileNode> parseDoWhileStmt() {
        auto stmt = std::make_unique<DoWhileNode>();
        stmt->line = previous().line;
        stmt->body = parseStmt();
        consume(TokenType::While, "Expected 'while'");
        consume(TokenType::LParen, "Expected '('");
        stmt->cond = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    std::unique_ptr<ForNode> parseForStmt() {
        auto stmt = std::make_unique<ForNode>();
        stmt->line = previous().line;
        consume(TokenType::LParen, "Expected '('");
        if (!check(TokenType::Semicolon)) {
            if (matchAny({TokenType::Let, TokenType::Const, TokenType::Var})) {
                stmt->init = parseVarDecl();
            } else {
                stmt->init = parseExprStmt();
            }
        } else { advance(); }
        if (!check(TokenType::Semicolon)) stmt->cond = parseExpr();
        consume(TokenType::Semicolon, "Expected ';'");
        if (!check(TokenType::RParen)) stmt->inc = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        stmt->body = parseStmt();
        return stmt;
    }

    std::unique_ptr<ReturnNode> parseReturnStmt() {
        auto stmt = std::make_unique<ReturnNode>();
        stmt->line = previous().line;
        if (!check(TokenType::Semicolon) && !check(TokenType::RBrace)) {
            stmt->value = parseExpr();
        }
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    std::unique_ptr<SwitchNode> parseSwitchStmt() {
        auto stmt = std::make_unique<SwitchNode>();
        stmt->line = previous().line;
        consume(TokenType::LParen, "Expected '('");
        stmt->expr = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        consume(TokenType::LBrace, "Expected '{'");
        while (!isAtEnd() && !check(TokenType::RBrace)) {
            if (match(TokenType::Case)) {
                auto caseExpr = parseExpr();
                consume(TokenType::Colon, "Expected ':'");
                std::vector<std::unique_ptr<Stmt>> body;
                while (!isAtEnd() && !check(TokenType::Case) && !check(TokenType::Default) && !check(TokenType::RBrace)) {
                    auto s = parseStmt();
                    if (s) body.push_back(std::move(s));
                }
                stmt->cases.emplace_back(std::move(caseExpr), std::move(body));
            } else if (match(TokenType::Default)) {
                consume(TokenType::Colon, "Expected ':'");
                while (!isAtEnd() && !check(TokenType::Case) && !check(TokenType::Default) && !check(TokenType::RBrace)) {
                    auto s = parseStmt();
                    if (s) stmt->defaultCase.push_back(std::move(s));
                }
            } else { error("Expected case or default"); break; }
        }
        consume(TokenType::RBrace, "Expected '}'");
        return stmt;
    }

    std::unique_ptr<ThrowNode> parseThrowStmt() {
        auto stmt = std::make_unique<ThrowNode>();
        stmt->line = previous().line;
        if (!check(TokenType::Semicolon) && !check(TokenType::RBrace)) {
            stmt->value = parseExpr();
        }
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    std::unique_ptr<TryNode> parseTryStmt() {
        auto stmt = std::make_unique<TryNode>();
        stmt->line = previous().line;
        consume(TokenType::LBrace, "Expected '{'");
        stmt->tryBlock = parseBlock();

        if (match(TokenType::Catch)) {
            consume(TokenType::LParen, "Expected '('");
            Token param = consume(TokenType::Identifier, "Expected catch parameter");
            stmt->catchParam = param.lexeme;
            consume(TokenType::RParen, "Expected ')'");
            consume(TokenType::LBrace, "Expected '{'");
            stmt->catchBlock = parseBlock();
        }

        if (match(TokenType::Finally)) {
            consume(TokenType::LBrace, "Expected '{'");
            stmt->finallyBlock = parseBlock();
        }

        if (!stmt->catchBlock && !stmt->finallyBlock) {
            error("Expected catch or finally");
        }

        return stmt;
    }

    std::unique_ptr<ExprStmtNode> parseExprStmt() {
        auto stmt = std::make_unique<ExprStmtNode>();
        stmt->line = peek().line;
        stmt->expr = parseExpr();
        consume(TokenType::Semicolon, "Expected ';'");
        return stmt;
    }

    std::unique_ptr<Expr> parseExpr() { return parseAssignment(); }

    std::unique_ptr<Expr> parseAssignment() {
        auto expr = parseConditional();
        if (matchAny({TokenType::Eq, TokenType::PlusEq, TokenType::MinusEq,
                      TokenType::StarEq, TokenType::SlashEq, TokenType::PercentEq})) {
            auto a = std::make_unique<AssignNode>();
            a->line = previous().line;
            a->op = previous().lexeme;
            a->target = std::move(expr);
            a->value = parseAssignment();
            return a;
        }
        return expr;
    }

    std::unique_ptr<Expr> parseConditional() {
        auto expr = parseOr();
        if (match(TokenType::Question)) {
            auto c = std::make_unique<ConditionalNode>();
            c->line = previous().line;
            c->cond = std::move(expr);
            c->thenExpr = parseExpr();
            consume(TokenType::Colon, "Expected ':'");
            c->elseExpr = parseConditional();
            return c;
        }
        return expr;
    }

    std::unique_ptr<Expr> parseOr() {
        auto expr = parseAnd();
        while (match(TokenType::OrOr)) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = "||";
            b->left = std::move(expr);
            b->right = parseAnd();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseAnd() {
        auto expr = parseEquality();
        while (match(TokenType::AndAnd)) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = "&&";
            b->left = std::move(expr);
            b->right = parseEquality();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseEquality() {
        auto expr = parseComparison();
        while (matchAny({TokenType::EqEq, TokenType::EqEqEq, TokenType::NotEq, TokenType::NotEqEq})) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = previous().lexeme;
            b->left = std::move(expr);
            b->right = parseComparison();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseComparison() {
        auto expr = parseTerm();
        while (matchAny({TokenType::Less, TokenType::Greater, TokenType::LessEq, TokenType::GreaterEq})) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = previous().lexeme;
            b->left = std::move(expr);
            b->right = parseTerm();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseTerm() {
        auto expr = parseFactor();
        while (matchAny({TokenType::Plus, TokenType::Minus})) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = previous().lexeme;
            b->left = std::move(expr);
            b->right = parseFactor();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parseFactor() {
        auto expr = parsePower();
        while (matchAny({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = previous().lexeme;
            b->left = std::move(expr);
            b->right = parsePower();
            expr = std::move(b);
        }
        return expr;
    }

    std::unique_ptr<Expr> parsePower() {
        auto expr = parseUnary();
        if (match(TokenType::DoubleStar)) {
            auto b = std::make_unique<BinaryExprNode>();
            b->line = previous().line;
            b->op = "**";
            b->left = std::move(expr);
            b->right = parsePower();
            return b;
        }
        return expr;
    }

    std::unique_ptr<Expr> parseUnary() {
        if (matchAny({TokenType::Not, TokenType::Minus, TokenType::Plus,
                      TokenType::Typeof, TokenType::PlusPlus, TokenType::MinusMinus})) {
            auto u = std::make_unique<UnaryExprNode>();
            u->line = previous().line;
            u->op = previous().lexeme;
            u->prefix = true;
            u->operand = parseUnary();
            return u;
        }
        return parsePostfix();
    }

    std::unique_ptr<Expr> parsePostfix() {
        auto expr = parseCall();
        if (matchAny({TokenType::PlusPlus, TokenType::MinusMinus})) {
            auto u = std::make_unique<UnaryExprNode>();
            u->line = previous().line;
            u->op = previous().lexeme;
            u->prefix = false;
            u->operand = std::move(expr);
            return u;
        }
        return expr;
    }

    std::unique_ptr<Expr> parseCall() {
        auto expr = parseMember();
        while (true) {
            if (match(TokenType::LParen)) {
                auto call = std::make_unique<CallExprNode>();
                call->line = previous().line;
                call->callee = std::move(expr);
                while (!isAtEnd() && !check(TokenType::RParen)) {
                    call->args.push_back(parseExpr());
                    if (!match(TokenType::Comma)) break;
                }
                consume(TokenType::RParen, "Expected ')'");
                expr = std::move(call);
            } else if (match(TokenType::Dot)) {
                auto mem = std::make_unique<MemberExprNode>();
                mem->line = previous().line;
                mem->object = std::move(expr);
                Token prop = consume(TokenType::Identifier, "Expected property");
                auto id = std::make_unique<IdentifierNode>();
                id->line = prop.line;
                id->name = prop.lexeme;
                mem->property = std::move(id);
                mem->computed = false;
                expr = std::move(mem);
            } else if (match(TokenType::LBracket)) {
                auto mem = std::make_unique<MemberExprNode>();
                mem->line = previous().line;
                mem->object = std::move(expr);
                mem->property = parseExpr();
                consume(TokenType::RBracket, "Expected ']'");
                mem->computed = true;
                expr = std::move(mem);
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<Expr> parseMember() {
        if (match(TokenType::New)) {
            auto expr = parsePrimary();
            while (true) {
                if (match(TokenType::Dot)) {
                    auto mem = std::make_unique<MemberExprNode>();
                    mem->line = previous().line;
                    mem->object = std::move(expr);
                    Token prop = consume(TokenType::Identifier, "Expected property");
                    auto id = std::make_unique<IdentifierNode>();
                    id->line = prop.line;
                    id->name = prop.lexeme;
                    mem->property = std::move(id);
                    mem->computed = false;
                    expr = std::move(mem);
                } else if (match(TokenType::LBracket)) {
                    auto mem = std::make_unique<MemberExprNode>();
                    mem->line = previous().line;
                    mem->object = std::move(expr);
                    mem->property = parseExpr();
                    consume(TokenType::RBracket, "Expected ']'");
                    mem->computed = true;
                    expr = std::move(mem);
                } else {
                    break;
                }
            }
            if (match(TokenType::LParen)) {
                auto newExpr = std::make_unique<NewExprNode>();
                newExpr->line = previous().line;
                newExpr->callee = std::move(expr);
                while (!isAtEnd() && !check(TokenType::RParen)) {
                    newExpr->args.push_back(parseExpr());
                    if (!match(TokenType::Comma)) break;
                }
                consume(TokenType::RParen, "Expected ')'");
                return newExpr;
            }
            auto newExpr = std::make_unique<NewExprNode>();
            newExpr->line = previous().line;
            newExpr->callee = std::move(expr);
            return newExpr;
        }
        return parsePrimary();
    }

    std::unique_ptr<Expr> parsePrimary() {
        if (match(TokenType::Number)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeNum(std::stod(previous().lexeme));
            return l;
        }
        if (match(TokenType::String)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeStr(previous().lexeme);
            return l;
        }
        if (match(TokenType::True)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeBool(true);
            return l;
        }
        if (match(TokenType::False)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeBool(false);
            return l;
        }
        if (match(TokenType::Null)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeNull();
            return l;
        }
        if (match(TokenType::Undefined)) {
            auto l = std::make_unique<LiteralNode>();
            l->line = previous().line;
            l->value = Value::makeUndefined();
            return l;
        }
        if (match(TokenType::This)) {
            auto id = std::make_unique<IdentifierNode>();
            id->line = previous().line;
            id->name = "this";
            return id;
        }
        if (match(TokenType::Identifier)) {
            auto id = std::make_unique<IdentifierNode>();
            id->line = previous().line;
            id->name = previous().lexeme;

            // Check for arrow function: id =>
            if (match(TokenType::FatArrow)) {
                auto af = std::make_unique<ArrowFuncNode>();
                af->line = id->line;
                af->params = {id->name};
                af->isExprBody = !check(TokenType::LBrace);
                if (match(TokenType::LBrace)) {
                    af->body = parseBlock();
                } else {
                    af->exprBody = parseExpr();
                }
                return af;
            }
            return id;
        }
        if (match(TokenType::LParen)) {
            return parseParenOrArrow();
        }
        if (match(TokenType::LBracket)) {
            auto arr = std::make_unique<ArrayLitNode>();
            arr->line = previous().line;
            while (!isAtEnd() && !check(TokenType::RBracket)) {
                if (check(TokenType::Comma)) { advance(); arr->elements.push_back(nullptr); continue; }
                arr->elements.push_back(parseExpr());
                match(TokenType::Comma);
            }
            consume(TokenType::RBracket, "Expected ']'");
            return arr;
        }
        if (match(TokenType::LBrace)) {
            auto obj = std::make_unique<ObjectLitNode>();
            obj->line = previous().line;
            while (!isAtEnd() && !check(TokenType::RBrace)) {
                TokenType keyType = peek().type;
                if (keyType != TokenType::String && keyType != TokenType::Identifier) {
                    error("Expected property name");
                    break;
                }
                Token key = advance();
                consume(TokenType::Colon, "Expected ':'");
                obj->properties.emplace_back(key.lexeme, parseExpr());
                match(TokenType::Comma);
            }
            consume(TokenType::RBrace, "Expected '}'");
            return obj;
        }
        if (match(TokenType::Function)) {
            auto af = std::make_unique<ArrowFuncNode>();
            af->isFuncExpr = true;
            af->line = previous().line;
            consume(TokenType::LParen, "Expected '('");
            if (!check(TokenType::RParen)) {
                do {
                    Token p = consume(TokenType::Identifier, "Expected param");
                    af->params.push_back(p.lexeme);
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')'");
            consume(TokenType::LBrace, "Expected '{'");
            af->body = parseBlock();
            af->isExprBody = false;
            return af;
        }

        error("Expected expression");
        auto l = std::make_unique<LiteralNode>();
        l->line = peek().line;
        l->value = Value::makeUndefined();
        return l;
    }

    std::unique_ptr<Expr> parseParenOrArrow() {
        // Could be (expr) or (params) => body
        size_t save = pos;

        if (check(TokenType::RParen)) {
            // () => - zero-param arrow function
            size_t s2 = pos;
            advance(); // consume )
            if (match(TokenType::FatArrow)) {
                auto af = std::make_unique<ArrowFuncNode>();
                af->line = peek().line;
                af->isExprBody = !check(TokenType::LBrace);
                if (match(TokenType::LBrace)) {
                    af->body = parseBlock();
                } else {
                    af->exprBody = parseExpr();
                }
                return af;
            }
            pos = s2; // not an arrow, restore
        } else {
            std::vector<std::string> params;
            size_t s2 = pos;
            bool allIdentifiers = true;
            while (!isAtEnd() && !check(TokenType::RParen)) {
                if (check(TokenType::Identifier)) {
                    params.push_back(peek().lexeme);
                    advance();
                } else { allIdentifiers = false; break; }
                if (!match(TokenType::Comma)) break;
            }
            if (allIdentifiers && match(TokenType::RParen) && match(TokenType::FatArrow)) {
                auto af = std::make_unique<ArrowFuncNode>();
                af->line = peek().line;
                af->params = params;
                af->isExprBody = !check(TokenType::LBrace);
                if (match(TokenType::LBrace)) {
                    af->body = parseBlock();
                } else {
                    af->exprBody = parseExpr();
                }
                return af;
            }
            pos = s2;
        }
        pos = save;

        // ( was already consumed by match(LParen) in parsePrimary
        auto expr = parseExpr();
        consume(TokenType::RParen, "Expected ')'");
        return expr;
    }
};

}
