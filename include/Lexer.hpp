#pragma once

#include "Token.hpp"

#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(std::string source);

    // Scan the complete source buffer and append a final EndOfFile token.
    std::vector<Token> scanTokens();

    // Scan until a token of the requested type is emitted, or until EndOfFile.
    // The stopping token is included; EndOfFile is included only when reached.
    std::vector<Token> scanTokensUntil(TokenType stopType);

private:
    bool isAtEnd() const;
    char advance();
    bool match(char expected);
    char peek() const;
    char peekNext() const;

    void scanToken();
    void addToken(TokenType type);
    void stringLiteral();
    void numberLiteral();
    void identifier();

    std::string source_;
    std::vector<Token> tokens_;
    // start_ marks the beginning of the lexeme currently being scanned;
    // current_ points to the next character to consume.
    std::size_t start_ = 0;
    std::size_t current_ = 0;
    int line_ = 1;
    int column_ = 1;
    int tokenColumn_ = 1;
};
