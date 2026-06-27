#include "Parser.hpp"

#include <optional>
#include <sstream>

ParseError::ParseError(const Token& token, const std::string& message)
    : std::runtime_error("Parse error at line " + std::to_string(token.line)
          + ", column " + std::to_string(token.column) + ": " + message)
{
}

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens))
{
}

Program Parser::parse()
{
    Program program;
    while (!isAtEnd()) {
        program.statements.push_back(declaration());
    }
    return program;
}

StmtPtr Parser::declaration()
{
    if (match(TokenType::Let)) {
        return letDeclaration();
    }
    return statement();
}

StmtPtr Parser::letDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected variable name after `let`");

    std::optional<Token> typeName;
    if (match(TokenType::Colon)) {
        typeName = consume(TokenType::Identifier, "expected type name after `:`");
    }

    consume(TokenType::Equal, "expected `=` after variable declaration");
    ExprPtr initializer = expression();

    consume(TokenType::Semicolon, "expected `;` after variable declaration");
    return std::make_unique<LetStmt>(std::move(name), std::move(typeName), std::move(initializer));
}

StmtPtr Parser::statement()
{
    if (match(TokenType::Print)) {
        return printStatement();
    }
    if (match(TokenType::If)) {
        return ifStatement();
    }
    if (match(TokenType::LeftBrace)) {
        return blockStatement();
    }
    return expressionStatement();
}

StmtPtr Parser::ifStatement()
{
    ExprPtr condition = expression();
    consume(TokenType::LeftBrace, "expected `{` after if condition");
    StmtPtr thenBranch = blockStatement();

    StmtPtr elseBranch;
    if (match(TokenType::Else)) {
        consume(TokenType::LeftBrace, "expected `{` after `else`");
        elseBranch = blockStatement();
    }

    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::blockStatement()
{
    return std::make_unique<BlockStmt>(blockStatements());
}

std::vector<StmtPtr> Parser::blockStatements()
{
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        statements.push_back(declaration());
    }
    consume(TokenType::RightBrace, "expected `}` after block");
    return statements;
}

StmtPtr Parser::printStatement()
{
    ExprPtr value = expression();
    consume(TokenType::Semicolon, "expected `;` after print value");
    return std::make_unique<PrintStmt>(std::move(value));
}

StmtPtr Parser::expressionStatement()
{
    ExprPtr value = expression();
    consume(TokenType::Semicolon, "expected `;` after expression");
    return std::make_unique<ExpressionStmt>(std::move(value));
}

ExprPtr Parser::expression()
{
    return equality();
}

ExprPtr Parser::equality()
{
    ExprPtr expr = comparison();
    // Left-associative binary operators are folded as they are encountered:
    // `a == b != c` becomes `(!= (== a b) c)`.
    while (match(TokenType::BangEqual) || match(TokenType::EqualEqual)) {
        Token op = previous();
        ExprPtr right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::comparison()
{
    ExprPtr expr = term();
    while (match(TokenType::Greater) || match(TokenType::GreaterEqual)
        || match(TokenType::Less) || match(TokenType::LessEqual)) {
        Token op = previous();
        ExprPtr right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::term()
{
    ExprPtr expr = factor();
    while (match(TokenType::Minus) || match(TokenType::Plus)) {
        Token op = previous();
        ExprPtr right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::factor()
{
    ExprPtr expr = unary();
    while (match(TokenType::Slash) || match(TokenType::Star)) {
        Token op = previous();
        ExprPtr right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::unary()
{
    if (match(TokenType::Bang) || match(TokenType::Minus)) {
        Token op = previous();
        ExprPtr right = unary();
        return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
    }

    return primary();
}

ExprPtr Parser::primary()
{
    if (match(TokenType::False)) {
        return std::make_unique<LiteralExpr>("false");
    }
    if (match(TokenType::True)) {
        return std::make_unique<LiteralExpr>("true");
    }
    if (match(TokenType::Nil)) {
        return std::make_unique<LiteralExpr>("nil");
    }
    if (match(TokenType::Number) || match(TokenType::String)) {
        return std::make_unique<LiteralExpr>(previous().lexeme);
    }
    if (match(TokenType::Identifier)) {
        return std::make_unique<VariableExpr>(previous());
    }
    if (match(TokenType::LeftParen)) {
        ExprPtr expr = expression();
        consume(TokenType::RightParen, "expected `)` after expression");
        return std::make_unique<GroupingExpr>(std::move(expr));
    }

    throw ParseError(peek(), "expected expression");
}

bool Parser::match(TokenType type)
{
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::check(TokenType type) const
{
    if (isAtEnd()) {
        return type == TokenType::EndOfFile;
    }
    return peek().type == type;
}

Token Parser::advance()
{
    if (!isAtEnd()) {
        current_++;
    }
    return previous();
}

bool Parser::isAtEnd() const
{
    return peek().type == TokenType::EndOfFile;
}

const Token& Parser::peek() const
{
    return tokens_[current_];
}

const Token& Parser::previous() const
{
    return tokens_[current_ - 1];
}

Token Parser::consume(TokenType type, const std::string& message)
{
    if (check(type)) {
        return advance();
    }

    // Include both the expected construct and the actual token to make syntax
    // errors actionable for users of the command-line demo.
    std::ostringstream fullMessage;
    fullMessage << message << ", found " << tokenTypeName(peek().type);
    if (!peek().lexeme.empty()) {
        fullMessage << " `" << peek().lexeme << "`";
    }
    throw ParseError(peek(), fullMessage.str());
}
