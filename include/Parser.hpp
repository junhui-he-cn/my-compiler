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
    StmtPtr exportDeclaration();
    StmtPtr structDeclaration();
    std::vector<StructFieldDecl> structFields();
    StructFieldDecl structField();
    StmtPtr functionDeclaration();
    StmtPtr letDeclaration();
    StmtPtr importDeclaration();
    std::vector<Parameter> parameters();
    Parameter parameter();
    TypeAnnotation typeAnnotation(const std::string& simpleTypeMessage = "expected type name");
    std::vector<TypeAnnotation> typeArguments();
    std::optional<TypeAnnotation> optionalReturnType();
    StmtPtr statement();
    StmtPtr ifStatement();
    StmtPtr forStatement();
    StmtPtr forInStatement(Token keyword, Token variable);
    StmtPtr forInitializer();
    StmtPtr whileStatement();
    StmtPtr breakStatement();
    StmtPtr continueStatement();
    StmtPtr blockStatement();
    std::vector<StmtPtr> blockStatements();
    StmtPtr printStatement();
    StmtPtr returnStatement();
    StmtPtr letDeclarationNoSemicolon(const std::string& terminatorMessage);
    StmtPtr expressionStatementNoSemicolon();
    StmtPtr expressionStatement();

    // Recursive descent expression grammar, ordered from lowest to highest precedence.
    ExprPtr expression();
    ExprPtr conditionExpression();
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
    ExprPtr arrayLiteral(Token bracket);
    std::vector<StructField> structLiteralFields();
    ExprPtr structLiteral();
    ExprPtr structConstructor();
    ExprPtr qualifiedStructConstructor();
    bool isQualifiedStructConstructorStart() const;
    ExprPtr functionExpression();
    ExprPtr primary();

    bool match(TokenType type);
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    Token advance();
    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    Token consume(TokenType type, const std::string& message);

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
    bool allowStructConstructors_ = true;
    int blockDepth_ = 0;
};
