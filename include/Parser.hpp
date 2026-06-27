#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <stdexcept>
#include <string>
#include <vector>

class ParseError final : public std::runtime_error {
public:
    explicit ParseError(const Token& token, const std::string& message);
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parse all tokens into a Program AST.
    Program parse();

private:
    StmtPtr declaration();
    StmtPtr letDeclaration();
    StmtPtr statement();
    StmtPtr ifStatement();
    StmtPtr blockStatement();
    std::vector<StmtPtr> blockStatements();
    StmtPtr printStatement();
    StmtPtr expressionStatement();

    // Recursive descent expression grammar, ordered from lowest to highest precedence.
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr primary();

    bool match(TokenType type);
    bool check(TokenType type) const;
    Token advance();
    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    Token consume(TokenType type, const std::string& message);

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};
