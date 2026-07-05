#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Token.hpp"

#include <string>
#include <vector>

class ParseError final : public DiagnosticError {
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
    StmtPtr structDeclaration();
    std::vector<StructFieldDecl> structFields();
    StructFieldDecl structField();
    StmtPtr functionDeclaration();
    StmtPtr letDeclaration();
    std::vector<Parameter> parameters();
    Parameter parameter();
    TypeAnnotation typeAnnotation(const std::string& simpleTypeMessage = "expected type name");
    std::vector<TypeAnnotation> typeArguments();
    std::optional<TypeAnnotation> optionalReturnType();
    StmtPtr statement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr breakStatement();
    StmtPtr continueStatement();
    StmtPtr blockStatement();
    std::vector<StmtPtr> blockStatements();
    StmtPtr printStatement();
    StmtPtr returnStatement();
    StmtPtr expressionStatement();

    // Recursive descent expression grammar, ordered from lowest to highest precedence.
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr finishCall(ExprPtr callee);
    ExprPtr finishIndex(ExprPtr collection);
    ExprPtr finishFieldAccess(ExprPtr object);
    ExprPtr arrayLiteral();
    ExprPtr structLiteral();
    ExprPtr functionExpression();
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
