#include "TypeChecker.hpp"

#include <stdexcept>
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

StaticType logicalResultType(StaticType left, StaticType right)
{
    if (!isKnown(left) || !isKnown(right)) {
        return StaticType::Unknown;
    }
    if (left == right) {
        return left;
    }
    return StaticType::Unknown;
}

std::string binaryTypesMessage(const BinaryExpr& expression, StaticType left, StaticType right)
{
    return "binary `" + expression.op.lexeme + "` expects numbers, got "
        + staticTypeName(left) + " and " + staticTypeName(right);
}

} // namespace

TypeError::TypeError(std::string message)
    : DiagnosticError(DiagnosticKind::Type, std::move(message))
{
}

TypeError::TypeError(const Token& token, std::string message)
    : DiagnosticError(DiagnosticKind::Type, SourceLocation{token.line, token.column}, std::move(message))
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
    case StaticType::Function:
        return "function";
    }

    return "unknown";
}


const std::string& ResolvedNames::letName(const LetStmt& statement) const
{
    const auto found = letNames_.find(&statement);
    if (found == letNames_.end()) {
        throw std::logic_error("missing resolved let name");
    }
    return found->second;
}

const std::string& ResolvedNames::functionName(const FunctionStmt& statement) const
{
    const auto found = functionNames_.find(&statement);
    if (found == functionNames_.end()) {
        throw std::logic_error("missing resolved function name");
    }
    return found->second;
}

const std::vector<std::string>& ResolvedNames::parameterNames(const FunctionStmt& statement) const
{
    const auto found = parameterNames_.find(&statement);
    if (found == parameterNames_.end()) {
        throw std::logic_error("missing resolved parameter names");
    }
    return found->second;
}

const std::string& ResolvedNames::variableName(const VariableExpr& expression) const
{
    const auto found = variableNames_.find(&expression);
    if (found == variableNames_.end()) {
        throw std::logic_error("missing resolved variable name");
    }
    return found->second;
}

const std::string& ResolvedNames::assignmentName(const AssignExpr& expression) const
{
    const auto found = assignmentNames_.find(&expression);
    if (found == assignmentNames_.end()) {
        throw std::logic_error("missing resolved assignment name");
    }
    return found->second;
}

void ResolvedNames::clear()
{
    letNames_.clear();
    functionNames_.clear();
    parameterNames_.clear();
    variableNames_.clear();
    assignmentNames_.clear();
}

void ResolvedNames::recordLet(const LetStmt& statement, std::string name)
{
    letNames_.emplace(&statement, std::move(name));
}

void ResolvedNames::recordFunction(const FunctionStmt& statement, std::string name)
{
    functionNames_.emplace(&statement, std::move(name));
}

void ResolvedNames::recordParameters(const FunctionStmt& statement, std::vector<std::string> names)
{
    parameterNames_.emplace(&statement, std::move(names));
}

void ResolvedNames::recordVariable(const VariableExpr& expression, std::string name)
{
    variableNames_.emplace(&expression, std::move(name));
}

void ResolvedNames::recordAssignment(const AssignExpr& expression, std::string name)
{
    assignmentNames_.emplace(&expression, std::move(name));
}

const ResolvedNames& TypeChecker::check(const Program& program)
{
    scopes_.clear();
    resolvedNames_.clear();
    nextResolvedName_ = 0;
    functionDepth_ = 0;
    beginScope();
    for (const auto& statement : program.statements) {
        checkStatement(*statement);
    }
    endScope();
    return resolvedNames_;
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

TypeChecker::Binding* TypeChecker::findVariable(const std::string& name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const TypeChecker::Binding* TypeChecker::findVariable(const std::string& name) const
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        auto found = scope->find(name);
        if (found != scope->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

TypeChecker::Binding TypeChecker::declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }

    Binding binding{type, makeResolvedName(name.lexeme), arity, scopes_.size() - 1, functionDepth_};
    scope.emplace(name.lexeme, binding);
    return binding;
}

TypeChecker::Binding TypeChecker::declareVariable(const LetStmt& statement, StaticType type)
{
    Binding binding = declareVariable(statement.name, type);
    resolvedNames_.recordLet(statement, binding.resolvedName);
    return binding;
}

std::string TypeChecker::makeResolvedName(const std::string& sourceName)
{
    return sourceName + "#" + std::to_string(nextResolvedName_++);
}

void TypeChecker::checkStatement(const Stmt& statement)
{
    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        checkFunction(*function);
        return;
    }

    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (functionDepth_ == 0) {
            throw TypeError(returnStmt->keyword, "return outside function");
        }
        if (returnStmt->value) {
            checkExpression(*returnStmt->value);
        }
        return;
    }

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

    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        checkExpression(*whileStmt->condition);
        checkStatement(*whileStmt->body);
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const StaticType declared = checkLetInitializer(*let);
        declareVariable(*let, declared);
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

void TypeChecker::checkFunction(const FunctionStmt& statement)
{
    Binding functionBinding = declareVariable(statement.name, StaticType::Function, statement.parameters.size());
    resolvedNames_.recordFunction(statement, functionBinding.resolvedName);

    beginScope();
    ++functionDepth_;

    std::vector<std::string> parameterNames;
    for (const Token& parameter : statement.parameters) {
        Binding parameterBinding = declareVariable(parameter, StaticType::Unknown);
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(statement, std::move(parameterNames));

    for (const auto& child : statement.body) {
        checkStatement(*child);
    }

    --functionDepth_;
    endScope();
}

StaticType TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return StaticType::Unknown;
    }

    const StaticType declared = resolveAnnotation(*statement.typeName);
    checkAssignable(
        statement.name,
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
        const Binding* binding = findVariable(variable->name.lexeme);
        if (!binding) {
            throw TypeError(variable->name, "undefined variable `" + variable->name.lexeme + "`");
        }
        if (functionDepth_ > 0 && !isGlobalBinding(*binding) && !isCurrentFunctionBinding(*binding)) {
            throw TypeError(variable->name, "cannot capture local variable `" + variable->name.lexeme + "`");
        }
        resolvedNames_.recordVariable(*variable, binding->resolvedName);
        return binding->type;
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const StaticType value = checkExpression(*assign->value);
        Binding* target = findVariable(assign->name.lexeme);
        if (!target) {
            throw TypeError(assign->name, "undefined variable `" + assign->name.lexeme + "`");
        }
        if (functionDepth_ > 0 && !isGlobalBinding(*target) && !isCurrentFunctionBinding(*target)) {
            throw TypeError(assign->name, "cannot capture local variable `" + assign->name.lexeme + "`");
        }
        if (isKnown(target->type) && isKnown(value) && target->type != value) {
            throw TypeError(assign->name, "cannot assign " + staticTypeName(value) + " to `" + assign->name.lexeme
                + "` of type " + staticTypeName(target->type));
        }
        resolvedNames_.recordAssignment(*assign, target->resolvedName);
        return isKnown(target->type) ? target->type : value;
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

    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&expression)) {
        const StaticType left = checkExpression(*logical->left);
        const StaticType right = checkExpression(*logical->right);
        return logicalResultType(left, right);
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return checkCall(*call);
    }

    throw TypeError("unsupported expression node");
}

StaticType TypeChecker::checkCall(const CallExpr& expression)
{
    const StaticType callee = checkExpression(*expression.callee);
    for (const auto& argument : expression.arguments) {
        checkExpression(*argument);
    }

    if (callee != StaticType::Unknown && callee != StaticType::Function) {
        throw TypeError(expression.paren, "can only call functions");
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get())) {
        const Binding* binding = findVariable(variable->name.lexeme);
        if (binding && binding->arity && *binding->arity != expression.arguments.size()) {
            throw TypeError(expression.paren, "expected " + std::to_string(*binding->arity)
                + " arguments but got " + std::to_string(expression.arguments.size()));
        }
    }

    return StaticType::Unknown;
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
    throw TypeError(typeName, "unknown type `" + typeName.lexeme + "`");
}

void TypeChecker::checkAssignable(const Token& token, const std::string& context, StaticType expected, StaticType actual) const
{
    if (!compatible(expected, actual)) {
        throw TypeError(token, context);
    }
}

StaticType TypeChecker::checkUnary(const UnaryExpr& expression)
{
    const StaticType right = checkExpression(*expression.right);
    switch (expression.op.type) {
    case TokenType::Minus:
        if (isKnown(right) && right != StaticType::Number) {
            throw TypeError(expression.op, "unary `-` expects number, got " + staticTypeName(right));
        }
        return StaticType::Number;
    case TokenType::Bang:
        return StaticType::Bool;
    default:
        throw TypeError(expression.op, "unsupported unary operator `" + expression.op.lexeme + "`");
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
        throw TypeError(expression.op, "binary `+` expects two numbers or two strings, got "
            + staticTypeName(left) + " and " + staticTypeName(right));
    case TokenType::Minus:
    case TokenType::Star:
    case TokenType::Slash:
        if (!isKnown(left) || !isKnown(right)) {
            return StaticType::Number;
        }
        if (left != StaticType::Number || right != StaticType::Number) {
            throw TypeError(expression.op, binaryTypesMessage(expression, left, right));
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
            throw TypeError(expression.op, binaryTypesMessage(expression, left, right));
        }
        return StaticType::Bool;
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
        return StaticType::Bool;
    default:
        throw TypeError(expression.op, "unsupported binary operator `" + expression.op.lexeme + "`");
    }
}

bool TypeChecker::isGlobalBinding(const Binding& binding) const
{
    return binding.scopeDepth == 0;
}

bool TypeChecker::isCurrentFunctionBinding(const Binding& binding) const
{
    return binding.functionDepth == functionDepth_;
}
