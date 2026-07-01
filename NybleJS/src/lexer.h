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
    explicit Lexer(std::string source)
        : source(std::move(source)), pos(0), line(1), col(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < source.size()) {
            skipWhitespaceAndComments();
            if (pos >= source.size()) break;

            char c = source[pos];

            if (c == '"' || c == '\'') {
                tokens.push_back(readString(c));
                continue;
            }

            if (std::isalpha(c) || c == '_' || c == '$') {
                tokens.push_back(readIdentifier());
                continue;
            }

            if (std::isdigit(c)) {
                tokens.push_back(readNumber());
                continue;
            }

            tokens.push_back(readPunctuatorOrOperator());
        }
        tokens.emplace_back(TokenType::Eof, "", line, col);
        return tokens;
    }

private:
    std::string source;
    size_t pos;
    size_t line;
    size_t col;

    char peek() const {
        return pos < source.size() ? source[pos] : '\0';
    }

    char peekNext() const {
        return pos + 1 < source.size() ? source[pos + 1] : '\0';
    }

    char advance() {
        char c = source[pos++];
        col++;
        return c;
    }

    void skipWhitespaceAndComments() {
        while (pos < source.size()) {
            char c = source[pos];
            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
            } else if (c == '\n') {
                advance();
                line++;
                col = 1;
            } else if (c == '/' && peekNext() == '/') {
                while (pos < source.size() && source[pos] != '\n') advance();
            } else if (c == '/' && peekNext() == '*') {
                advance(); advance(); // skip /*
                while (pos < source.size()) {
                    if (source[pos] == '*' && peekNext() == '/') {
                        advance(); advance();
                        break;
                    }
                    if (source[pos] == '\n') { line++; col = 1; }
                    else advance();
                }
            } else {
                break;
            }
        }
    }

    Token readString(char delim) {
        size_t startLine = line;
        size_t startCol = col;
        advance(); // skip opening quote
        std::string val;
        while (pos < source.size()) {
            char c = advance();
            if (c == delim) {
                return {TokenType::String, val, startLine, startCol};
            }
            if (c == '\\') {
                if (pos < source.size()) {
                    char n = advance();
                    switch (n) {
                        case 'n': val += '\n'; break;
                        case 't': val += '\t'; break;
                        case 'r': val += '\r'; break;
                        case '0': val += '\0'; break;
                        case '\\': val += '\\'; break;
                        case '"': val += '"'; break;
                        case '\'': val += '\''; break;
                        default: val += '\\'; val += n; break;
                    }
                }
            } else {
                if (c == '\n') line++;
                val += c;
            }
        }
        return {TokenType::String, val, startLine, startCol};
    }

    Token readIdentifier() {
        size_t startLine = line;
        size_t startCol = col;
        std::string id;
        while (pos < source.size() && (std::isalnum(source[pos]) || source[pos] == '_' || source[pos] == '$')) {
            id += advance();
        }

        static const std::unordered_map<std::string, TokenType> keywords = {
            {"let", TokenType::Let},
            {"const", TokenType::Const},
            {"var", TokenType::Var},
            {"function", TokenType::Function},
            {"if", TokenType::If},
            {"else", TokenType::Else},
            {"while", TokenType::While},
            {"for", TokenType::For},
            {"do", TokenType::Do},
            {"return", TokenType::Return},
            {"break", TokenType::Break},
            {"continue", TokenType::Continue},
            {"switch", TokenType::Switch},
            {"case", TokenType::Case},
            {"default", TokenType::Default},
            {"true", TokenType::True},
            {"false", TokenType::False},
            {"null", TokenType::Null},
            {"undefined", TokenType::Undefined},
            {"typeof", TokenType::Typeof},
            {"new", TokenType::New},
            {"this", TokenType::This},
            {"class", TokenType::Class},

            {"try", TokenType::Try},
            {"catch", TokenType::Catch},
            {"finally", TokenType::Finally},
            {"throw", TokenType::Throw},
        };

        auto it = keywords.find(id);
        if (it != keywords.end()) {
            return {it->second, id, startLine, startCol};
        }
        return {TokenType::Identifier, id, startLine, startCol};
    }

    Token readNumber() {
        size_t startLine = line;
        size_t startCol = col;
        std::string num;
        while (pos < source.size() && std::isdigit(source[pos])) num += advance();
        if (peek() == '.' && std::isdigit(peekNext())) {
            num += advance(); // dot
            while (pos < source.size() && std::isdigit(source[pos])) num += advance();
        }
        if ((peek() == 'e' || peek() == 'E') && (std::isdigit(peekNext()) || peekNext() == '+' || peekNext() == '-')) {
            num += advance();
            if (peek() == '+' || peek() == '-') num += advance();
            while (pos < source.size() && std::isdigit(source[pos])) num += advance();
        }
        return {TokenType::Number, num, startLine, startCol};
    }

    Token readPunctuatorOrOperator() {
        size_t startLine = line;
        size_t startCol = col;
        char c = advance();

        switch (c) {
            case '(': return {TokenType::LParen, "(", startLine, startCol};
            case ')': return {TokenType::RParen, ")", startLine, startCol};
            case '{': return {TokenType::LBrace, "{", startLine, startCol};
            case '}': return {TokenType::RBrace, "}", startLine, startCol};
            case '[': return {TokenType::LBracket, "[", startLine, startCol};
            case ']': return {TokenType::RBracket, "]", startLine, startCol};
            case ',': return {TokenType::Comma, ",", startLine, startCol};
            case ';': return {TokenType::Semicolon, ";", startLine, startCol};
            case '?': return {TokenType::Question, "?", startLine, startCol};
            case ':': return {TokenType::Colon, ":", startLine, startCol};
            case '.': return {TokenType::Dot, ".", startLine, startCol};

            case '+':
                if (peek() == '+') { advance(); return {TokenType::PlusPlus, "++", startLine, startCol}; }
                if (peek() == '=') { advance(); return {TokenType::PlusEq, "+=", startLine, startCol}; }
                return {TokenType::Plus, "+", startLine, startCol};
            case '-':
                if (peek() == '-') { advance(); return {TokenType::MinusMinus, "--", startLine, startCol}; }
                if (peek() == '=') { advance(); return {TokenType::MinusEq, "-=", startLine, startCol}; }
                if (peek() == '>') { advance(); return {TokenType::ArrowRight, "->", startLine, startCol}; }
                return {TokenType::Minus, "-", startLine, startCol};
            case '*':
                if (peek() == '*') { advance(); return {TokenType::DoubleStar, "**", startLine, startCol}; }
                if (peek() == '=') { advance(); return {TokenType::StarEq, "*=", startLine, startCol}; }
                return {TokenType::Star, "*", startLine, startCol};
            case '/':
                if (peek() == '=') { advance(); return {TokenType::SlashEq, "/=", startLine, startCol}; }
                return {TokenType::Slash, "/", startLine, startCol};
            case '%':
                if (peek() == '=') { advance(); return {TokenType::PercentEq, "%=", startLine, startCol}; }
                return {TokenType::Percent, "%", startLine, startCol};

            case '=':
                if (peek() == '>') { advance(); return {TokenType::FatArrow, "=>", startLine, startCol}; }
                if (peek() == '=') { advance(); return peek() == '=' ? (advance(), Token{TokenType::EqEqEq, "===", startLine, startCol}) : Token{TokenType::EqEq, "==", startLine, startCol}; }
                return {TokenType::Eq, "=", startLine, startCol};
            case '!':
                if (peek() == '=') { advance(); return peek() == '=' ? (advance(), Token{TokenType::NotEqEq, "!==", startLine, startCol}) : Token{TokenType::NotEq, "!=", startLine, startCol}; }
                return {TokenType::Not, "!", startLine, startCol};
            case '<':
                if (peek() == '=') { advance(); return {TokenType::LessEq, "<=", startLine, startCol}; }
                return {TokenType::Less, "<", startLine, startCol};
            case '>':
                if (peek() == '=') { advance(); return {TokenType::GreaterEq, ">=", startLine, startCol}; }
                return {TokenType::Greater, ">", startLine, startCol};
            case '&':
                if (peek() == '&') { advance(); return {TokenType::AndAnd, "&&", startLine, startCol}; }
                return {TokenType::And, "&", startLine, startCol};
            case '|':
                if (peek() == '|') { advance(); return {TokenType::OrOr, "||", startLine, startCol}; }
                return {TokenType::Or, "|", startLine, startCol};
            case '~': return {TokenType::Invalid, "~", startLine, startCol};
            default: return {TokenType::Invalid, std::string(1, c), startLine, startCol};
        }
    }
};

}
