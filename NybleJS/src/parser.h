#pragma once
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include "lexer.h"
#include "ast.h"

namespace nyble {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse();
    std::vector<std::string> getErrors() const;

private:
    std::vector<Token> tokens;
    size_t pos;
    std::vector<std::string> errors;

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& msg);
    void error(const std::string& msg);

    std::unique_ptr<Stmt> parseStmt();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<VarDeclNode> parseVarDecl();
    std::unique_ptr<FunDeclNode> parseFunDecl();
    std::unique_ptr<IfNode> parseIfStmt();
    std::unique_ptr<WhileNode> parseWhileStmt();
    std::unique_ptr<DoWhileNode> parseDoWhileStmt();
    std::unique_ptr<ForNode> parseForStmt();
    std::unique_ptr<ReturnNode> parseReturnStmt();
    std::unique_ptr<SwitchNode> parseSwitchStmt();
    std::unique_ptr<ThrowNode> parseThrowStmt();
    std::unique_ptr<TryNode> parseTryStmt();
    std::unique_ptr<ExprStmtNode> parseExprStmt();

    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseAssignment();
    std::unique_ptr<Expr> parseConditional();
    std::unique_ptr<Expr> parseOr();
    std::unique_ptr<Expr> parseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
    std::unique_ptr<Expr> parsePower();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfix();
    std::unique_ptr<Expr> parseCall();
    std::unique_ptr<Expr> parseMember();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseParenOrArrow();
};

}
