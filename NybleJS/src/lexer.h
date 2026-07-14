#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

namespace nyble {

enum class TokenType {
    // Literals
    Number, String, Identifier,

    // Keywords
    Let, Const, Var, Function, If, Else, While, For, Do,
    Return, Break, Continue, Switch, Case, Default,
    True, False, Null, Undefined, Typeof, New, This,
    Class, Of, In, VarArg,
    Try, Catch, Finally, Throw, ConstLet, Arrow,

    // Operators
    Plus, Minus, Star, Slash, Percent, DoubleStar,
    PlusPlus, MinusMinus,
    PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
    Eq, EqEq, EqEqEq, NotEq, NotEqEq,
    Less, Greater, LessEq, GreaterEq,
    And, Or, Not,
    AndAnd, OrOr,
    Question, Colon,
    FatArrow, ArrowRight,

    // Punctuation
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Dot, Semicolon,

    // Special
    Eof, Invalid
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
    size_t col;

    Token() : type(TokenType::Eof), line(0), col(0) {}
    Token(TokenType t, std::string lx, size_t l, size_t c)
        : type(t), lexeme(std::move(lx)), line(l), col(c) {}
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos;
    size_t line;
    size_t col;

    char peek() const;
    char peekNext() const;
    char advance();
    void skipWhitespaceAndComments();
    Token readString(char delim);
    Token readIdentifier();
    Token readNumber();
    Token readPunctuatorOrOperator();
};

}
