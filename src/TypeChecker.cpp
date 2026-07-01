#include "TypeChecker.hpp"

#include <utility>

namespace {

bool isKnown(StaticType type)
{
    return type != StaticType::Unknown;
}

bool compatible(StaticType expected, StaticType actual)
{
    return !isKnown(expected) || !isKnown(actual) || expected == actual;
}

std::string binaryTypesMessage(const BinaryExpr& expression, StaticType left, StaticType right)
{
    return "binary `" + expression.op.lexeme + "` expects numbers, got "
        + staticTypeName(left) + " and " + staticTypeName(right);
}

} // namespace

TypeError::TypeError(const std::string& message)
    : std::runtime_error("Type error: " + message)
{
}

std::string staticTypeName(StaticType type)
{
    switch (type) {
    case StaticType::Unknown:
        return "unknown";
    case StaticType::Nil:
        return "nil";
    case StaticType::Number:
        return "number";
    case StaticType::Bool:
        return "bool";
    case StaticType::String:
        return "string";
    }

    return "unknown";
}

void TypeChecker::check(const Program& program)
{
    scopes_.clear();
    beginScope();
    for (const auto& statement : program.statements) {
        checkStatement(*statement);
    }
    endScope();
}

void TypeChecker::beginScope()
{
    scopes_.emplace_back();
}

void TypeChecker::endScope()
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    scopes_.pop_back();
}

TypeChecker::Scope& TypeChecker::currentScope()
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    return scopes_.back();
}

const TypeChecker::Scope& TypeChecker::currentScope() const
{
    if (scopes_.empty()) {
        throw TypeError("scope stack is empty");
    }
    return scopes_.back();
}

StaticType* TypeChecker::findVariable(const std::string& name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const StaticType* TypeChecker::findVariable(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

void TypeChecker::declareVariable(const Token& name, StaticType type)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError("variable `" + name.lexeme + "` already declared in this scope");
    }
    scope.emplace(name.lexeme, type);
}

void TypeChecker::checkStatement(const Stmt& statement)
{
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        beginScope();
        for (const auto& child : block->statements) {
            checkStatement(*child);
        }
        endScope();
        return;
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
        checkExpression(*ifStmt->condition);
        checkStatement(*ifStmt->thenBranch);
        if (ifStmt->elseBranch) {
            checkStatement(*ifStmt->elseBranch);
        }
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(let->name, declared);
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        checkExpression(*print->expression);
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        checkExpression(*expression->expression);
        return;
    }

    throw TypeError("unsupported statement node");
}

StaticType TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return StaticType::Unknown;
    }

    const StaticType declared = resolveAnnotation(*statement.typeName);
    checkAssignable(
        "cannot initialize `" + statement.name.lexeme + "` of type " + staticTypeName(declared)
            + " with " + staticTypeName(initializer),
        declared,
        initializer);
    return declared;
}

StaticType TypeChecker::checkExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        if (literal->value == "nil") {
            return StaticType::Nil;
        }
        if (literal->value == "true" || literal->value == "false") {
            return StaticType::Bool;
        }
        if (literal->value.size() >= 2 && literal->value.front() == '"' && literal->value.back() == '"') {
            return StaticType::String;
        }
        return StaticType::Number;
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        const StaticType* type = findVariable(variable->name.lexeme);
        return type ? *type : StaticType::Unknown;
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const StaticType value = checkExpression(*assign->value);
        StaticType* target = findVariable(assign->name.lexeme);
        if (!target) {
            return value;
        }
        if (isKnown(*target) && isKnown(value) && *target != value) {
            throw TypeError("cannot assign " + staticTypeName(value) + " to `" + assign->name.lexeme
                + "` of type " + staticTypeName(*target));
        }
        return isKnown(*target) ? *target : value;
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        return checkExpression(*grouping->expression);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        return checkUnary(*unary);
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        return checkBinary(*binary);
    }

    throw TypeError("unsupported expression node");
}

StaticType TypeChecker::resolveAnnotation(const Token& typeName) const
{
    if (typeName.lexeme == "number") {
        return StaticType::Number;
    }
    if (typeName.lexeme == "bool") {
        return StaticType::Bool;
    }
    if (typeName.lexeme == "string") {
        return StaticType::String;
    }
    if (typeName.lexeme == "nil") {
        return StaticType::Nil;
    }
    throw TypeError("unknown type `" + typeName.lexeme + "`");
}

void TypeChecker::checkAssignable(const std::string& context, StaticType expected, StaticType actual) const
{
    if (!compatible(expected, actual)) {
        throw TypeError(context);
    }
}

StaticType TypeChecker::checkUnary(const UnaryExpr& expression)
{
    const StaticType right = checkExpression(*expression.right);
    switch (expression.op.type) {
    case TokenType::Minus:
        if (isKnown(right) && right != StaticType::Number) {
            throw TypeError("unary `-` expects number, got " + staticTypeName(right));
        }
        return StaticType::Number;
    case TokenType::Bang:
        return StaticType::Bool;
    default:
        throw TypeError("unsupported unary operator `" + expression.op.lexeme + "`");
    }
}

StaticType TypeChecker::checkBinary(const BinaryExpr& expression)
{
    const StaticType left = checkExpression(*expression.left);
    const StaticType right = checkExpression(*expression.right);

    switch (expression.op.type) {
    case TokenType::Plus:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Unknown;
        }
        if (left == StaticType::Number && right == StaticType::Number) {
            return StaticType::Number;
        }
        if (left == StaticType::String && right == StaticType::String) {
            return StaticType::String;
        }
        throw TypeError("binary `+` expects two numbers or two strings, got "
            + staticTypeName(left) + " and " + staticTypeName(right));
    case TokenType::Minus:
    case TokenType::Star:
    case TokenType::Slash:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Number;
        }
        if (left != StaticType::Number || right != StaticType::Number) {
            throw TypeError(binaryTypesMessage(expression, left, right));
        }
        return StaticType::Number;
    case TokenType::Greater:
    case TokenType::GreaterEqual:
    case TokenType::Less:
    case TokenType::LessEqual:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Bool;
        }
        if (left != StaticType::Number || right != StaticType::Number) {
            throw TypeError(binaryTypesMessage(expression, left, right));
        }
        return StaticType::Bool;
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
        return StaticType::Bool;
    default:
        throw TypeError("unsupported binary operator `" + expression.op.lexeme + "`");
    }
}
