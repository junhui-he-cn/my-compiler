#include "Parser.hpp"

#include <optional>
#include <sstream>
#include <utility>

ParseError::ParseError(const Token& token, const std::string& message)
    : DiagnosticError(DiagnosticKind::Parse, SourceLocation{token.line, token.column}, message)
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
    if (match(TokenType::Fun)) {
        return functionDeclaration();
    }
    if (match(TokenType::Let)) {
        return letDeclaration();
    }
    return statement();
}

StmtPtr Parser::functionDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected function name after `fun`");
    consume(TokenType::LeftParen, "expected `(` after function name");

    std::vector<Parameter> parsedParameters = parameters();
    consume(TokenType::RightParen, "expected `)` after function parameters");
    std::optional<TypeAnnotation> returnTypeName = optionalReturnType();
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionStmt>(
        std::move(name),
        std::move(parsedParameters),
        std::move(returnTypeName),
        blockStatements());
}

StmtPtr Parser::letDeclaration()
{
    Token name = consume(TokenType::Identifier, "expected variable name after `let`");

    std::optional<TypeAnnotation> typeName;
    if (match(TokenType::Colon)) {
        typeName = typeAnnotation("expected type name after `:`");
    }

    consume(TokenType::Equal, "expected `=` after variable declaration");
    ExprPtr initializer = expression();

    consume(TokenType::Semicolon, "expected `;` after variable declaration");
    return std::make_unique<LetStmt>(std::move(name), std::move(typeName), std::move(initializer));
}

std::vector<Parameter> Parser::parameters()
{
    std::vector<Parameter> parsedParameters;
    if (!check(TokenType::RightParen)) {
        do {
            parsedParameters.push_back(parameter());
        } while (match(TokenType::Comma));
    }
    return parsedParameters;
}

Parameter Parser::parameter()
{
    Token name = consume(TokenType::Identifier, "expected parameter name");
    std::optional<TypeAnnotation> typeName;
    if (match(TokenType::Colon)) {
        typeName = typeAnnotation("expected parameter type after `:`");
    }
    return Parameter{std::move(name), std::move(typeName)};
}

std::optional<TypeAnnotation> Parser::optionalReturnType()
{
    if (!match(TokenType::Colon)) {
        return std::nullopt;
    }
    return typeAnnotation("expected return type after `:`");
}

TypeAnnotation Parser::typeAnnotation(const std::string& simpleTypeMessage)
{
    if (match(TokenType::Fun)) {
        Token keyword = previous();
        consume(TokenType::LeftParen, "expected `(` after `fun` in function type");
        std::vector<TypeAnnotation> parameterTypes = typeArguments();
        consume(TokenType::RightParen, "expected `)` after function type parameters");
        consume(TokenType::Colon, "expected `:` before function type return");
        TypeAnnotation returnType = typeAnnotation();
        return TypeAnnotation::function(std::move(keyword), std::move(parameterTypes), std::move(returnType));
    }

    return TypeAnnotation::simple(consume(TokenType::Identifier, simpleTypeMessage));
}

std::vector<TypeAnnotation> Parser::typeArguments()
{
    std::vector<TypeAnnotation> arguments;
    if (!check(TokenType::RightParen)) {
        do {
            arguments.push_back(typeAnnotation());
        } while (match(TokenType::Comma));
    }
    return arguments;
}

StmtPtr Parser::statement()
{
    if (match(TokenType::Print)) {
        return printStatement();
    }
    if (match(TokenType::If)) {
        return ifStatement();
    }
    if (match(TokenType::While)) {
        return whileStatement();
    }
    if (match(TokenType::Break)) {
        return breakStatement();
    }
    if (match(TokenType::Continue)) {
        return continueStatement();
    }
    if (match(TokenType::Return)) {
        return returnStatement();
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

StmtPtr Parser::whileStatement()
{
    ExprPtr condition = expression();
    consume(TokenType::LeftBrace, "expected `{` after while condition");
    StmtPtr body = blockStatement();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
}

StmtPtr Parser::breakStatement()
{
    Token keyword = previous();
    consume(TokenType::Semicolon, "expected `;` after break");
    return std::make_unique<BreakStmt>(std::move(keyword));
}

StmtPtr Parser::continueStatement()
{
    Token keyword = previous();
    consume(TokenType::Semicolon, "expected `;` after continue");
    return std::make_unique<ContinueStmt>(std::move(keyword));
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

StmtPtr Parser::returnStatement()
{
    Token keyword = previous();
    ExprPtr value;
    if (!check(TokenType::Semicolon)) {
        value = expression();
    }
    consume(TokenType::Semicolon, "expected `;` after return value");
    return std::make_unique<ReturnStmt>(std::move(keyword), std::move(value));
}

StmtPtr Parser::expressionStatement()
{
    ExprPtr value = expression();
    consume(TokenType::Semicolon, "expected `;` after expression");
    return std::make_unique<ExpressionStmt>(std::move(value));
}

ExprPtr Parser::expression()
{
    return assignment();
}

ExprPtr Parser::assignment()
{
    ExprPtr expr = logicalOr();

    if (match(TokenType::Equal)) {
        Token equals = previous();
        ExprPtr value = assignment();

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(variable->name, std::move(value));
        }

        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            ExprPtr collection = std::move(index->collection);
            Token bracket = std::move(index->bracket);
            ExprPtr indexExpression = std::move(index->index);
            return std::make_unique<IndexAssignExpr>(
                std::move(collection), std::move(bracket), std::move(indexExpression), std::move(value));
        }

        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr.get())) {
            ExprPtr object = std::move(field->object);
            Token name = std::move(field->name);
            return std::make_unique<FieldAssignExpr>(std::move(object), std::move(name), std::move(value));
        }

        throw ParseError(equals, "invalid assignment target");
    }

    return expr;
}

ExprPtr Parser::logicalOr()
{
    ExprPtr expr = logicalAnd();
    while (match(TokenType::PipePipe)) {
        Token op = previous();
        ExprPtr right = logicalAnd();
        expr = std::make_unique<LogicalExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
}

ExprPtr Parser::logicalAnd()
{
    ExprPtr expr = equality();
    while (match(TokenType::AmpersandAmpersand)) {
        Token op = previous();
        ExprPtr right = equality();
        expr = std::make_unique<LogicalExpr>(std::move(expr), std::move(op), std::move(right));
    }
    return expr;
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

    return call();
}

ExprPtr Parser::call()
{
    ExprPtr expr = primary();
    while (true) {
        if (match(TokenType::LeftParen)) {
            expr = finishCall(std::move(expr));
        } else if (match(TokenType::LeftBracket)) {
            expr = finishIndex(std::move(expr));
        } else if (match(TokenType::Dot)) {
            expr = finishFieldAccess(std::move(expr));
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::finishCall(ExprPtr callee)
{
    std::vector<ExprPtr> arguments;
    if (!check(TokenType::RightParen)) {
        do {
            arguments.push_back(expression());
        } while (match(TokenType::Comma));
    }
    Token paren = consume(TokenType::RightParen, "expected `)` after arguments");
    return std::make_unique<CallExpr>(std::move(callee), std::move(paren), std::move(arguments));
}

ExprPtr Parser::finishIndex(ExprPtr collection)
{
    Token bracket = previous();
    ExprPtr index = expression();
    consume(TokenType::RightBracket, "expected `]` after index");
    return std::make_unique<IndexExpr>(std::move(collection), std::move(bracket), std::move(index));
}

ExprPtr Parser::finishFieldAccess(ExprPtr object)
{
    Token name = consume(TokenType::Identifier, "expected field name after `.`");
    return std::make_unique<FieldAccessExpr>(std::move(object), std::move(name));
}

ExprPtr Parser::arrayLiteral()
{
    std::vector<ExprPtr> elements;
    if (!check(TokenType::RightBracket)) {
        do {
            elements.push_back(expression());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBracket, "expected `]` after array elements");
    return std::make_unique<ArrayExpr>(std::move(elements));
}

ExprPtr Parser::structLiteral()
{
    std::vector<StructField> fields;
    if (!check(TokenType::RightBrace)) {
        do {
            Token name = consume(TokenType::Identifier, "expected struct field name");
            consume(TokenType::Colon, "expected `:` after struct field name");
            ExprPtr value = expression();
            fields.push_back(StructField{std::move(name), std::move(value)});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RightBrace, "expected `}` after struct fields");
    return std::make_unique<StructExpr>(std::move(fields));
}

ExprPtr Parser::functionExpression()
{
    Token keyword = previous();
    consume(TokenType::LeftParen, "expected `(` after `fun`");

    std::vector<Parameter> parsedParameters = parameters();
    consume(TokenType::RightParen, "expected `)` after function parameters");
    std::optional<TypeAnnotation> returnTypeName = optionalReturnType();
    consume(TokenType::LeftBrace, "expected `{` before function body");
    return std::make_unique<FunctionExpr>(
        std::move(keyword),
        std::move(parsedParameters),
        std::move(returnTypeName),
        blockStatements());
}

ExprPtr Parser::primary()
{
    if (match(TokenType::Fun)) {
        return functionExpression();
    }
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
    if (match(TokenType::LeftBracket)) {
        return arrayLiteral();
    }
    if (match(TokenType::LeftBrace)) {
        return structLiteral();
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
