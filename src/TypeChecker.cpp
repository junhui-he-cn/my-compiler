#include "TypeChecker.hpp"

#include <stdexcept>
#include <utility>

namespace {

TypeInfo unknownType()
{
    return TypeInfo{StaticType::Unknown, {}, nullptr};
}

TypeInfo simpleType(StaticType kind)
{
    return TypeInfo{kind, {}, nullptr};
}

TypeInfo functionType(std::vector<TypeInfo> parameterTypes, TypeInfo returnType)
{
    TypeInfo result;
    result.kind = StaticType::Function;
    result.parameterTypes = std::move(parameterTypes);
    result.returnType = std::make_shared<TypeInfo>(std::move(returnType));
    return result;
}

TypeInfo functionWithoutSignature()
{
    return TypeInfo{StaticType::Function, {}, nullptr};
}

bool isKnown(const TypeInfo& type)
{
    return type.kind != StaticType::Unknown;
}

bool hasFunctionSignature(const TypeInfo& type)
{
    return type.kind == StaticType::Function && type.returnType != nullptr;
}

std::string typeInfoName(const TypeInfo& type)
{
    if (type.kind != StaticType::Function || !type.returnType) {
        return staticTypeName(type.kind);
    }

    std::string result = "fun(";
    for (std::size_t i = 0; i < type.parameterTypes.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += typeInfoName(type.parameterTypes[i]);
    }
    result += "): ";
    result += typeInfoName(*type.returnType);
    return result;
}

bool compatible(const TypeInfo& expected, const TypeInfo& actual)
{
    if (!isKnown(expected) || !isKnown(actual)) {
        return true;
    }
    if (expected.kind != actual.kind) {
        return false;
    }
    if (expected.kind != StaticType::Function) {
        return true;
    }
    if (!hasFunctionSignature(expected) || !hasFunctionSignature(actual)) {
        return true;
    }
    if (expected.parameterTypes.size() != actual.parameterTypes.size()) {
        return false;
    }
    for (std::size_t i = 0; i < expected.parameterTypes.size(); ++i) {
        if (!compatible(expected.parameterTypes[i], actual.parameterTypes[i])) {
            return false;
        }
    }
    return compatible(*expected.returnType, *actual.returnType);
}

TypeInfo logicalResultType(const TypeInfo& left, const TypeInfo& right)
{
    if (!isKnown(left) || !isKnown(right)) {
        return unknownType();
    }
    if (left.kind == right.kind) {
        return left;
    }
    return unknownType();
}

TypeInfo mergeReturnTypes(const TypeInfo& current, const TypeInfo& next)
{
    if (compatible(current, next) && compatible(next, current) && current.kind == next.kind) {
        if (current.kind != StaticType::Function || typeInfoName(current) == typeInfoName(next)) {
            return current;
        }
    }
    if (!isKnown(current) || !isKnown(next)) {
        return unknownType();
    }
    return unknownType();
}

std::string binaryTypesMessage(const BinaryExpr& expression, const TypeInfo& left, const TypeInfo& right)
{
    return "binary `" + expression.op.lexeme + "` expects numbers, got "
        + typeInfoName(left) + " and " + typeInfoName(right);
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
    case StaticType::Array:
        return "array";
    case StaticType::Struct:
        return "struct";
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

const std::string& ResolvedNames::functionName(const FunctionExpr& expression) const
{
    const auto found = functionExpressionNames_.find(&expression);
    if (found == functionExpressionNames_.end()) {
        throw std::logic_error("missing resolved function expression name");
    }
    return found->second;
}

const std::vector<std::string>& ResolvedNames::parameterNames(const FunctionExpr& expression) const
{
    const auto found = functionExpressionParameterNames_.find(&expression);
    if (found == functionExpressionParameterNames_.end()) {
        throw std::logic_error("missing resolved function expression parameter names");
    }
    return found->second;
}

bool ResolvedNames::hasVariable(const VariableExpr& expression) const
{
    return variableNames_.find(&expression) != variableNames_.end();
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
    functionExpressionNames_.clear();
    functionExpressionParameterNames_.clear();
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

void ResolvedNames::recordFunction(const FunctionExpr& expression, std::string name)
{
    functionExpressionNames_.emplace(&expression, std::move(name));
}

void ResolvedNames::recordParameters(const FunctionExpr& expression, std::vector<std::string> names)
{
    functionExpressionParameterNames_.emplace(&expression, std::move(names));
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
    loopDepth_ = 0;
    returnContexts_.clear();
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

TypeChecker::Binding TypeChecker::declareVariable(
    const Token& name,
    TypeInfo type,
    bool explicitType)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }

    Binding binding{std::move(type), makeResolvedName(name.lexeme), scopes_.size() - 1, functionDepth_, explicitType};
    scope.emplace(name.lexeme, binding);
    return binding;
}

TypeChecker::Binding TypeChecker::declareVariable(
    const LetStmt& statement,
    TypeInfo type,
    bool explicitType)
{
    Binding binding = declareVariable(statement.name, std::move(type), explicitType);
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
        const TypeInfo returned = returnStmt->value
            ? checkExpression(*returnStmt->value)
            : simpleType(StaticType::Nil);
        recordReturn(returnStmt->keyword, returned);
        return;
    }

    if (const auto* breakStmt = dynamic_cast<const BreakStmt*>(&statement)) {
        if (loopDepth_ == 0) {
            throw TypeError(breakStmt->keyword, "`break` can only be used inside a loop");
        }
        return;
    }

    if (const auto* continueStmt = dynamic_cast<const ContinueStmt*>(&statement)) {
        if (loopDepth_ == 0) {
            throw TypeError(continueStmt->keyword, "`continue` can only be used inside a loop");
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
        ++loopDepth_;
        checkStatement(*whileStmt->body);
        --loopDepth_;
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const CheckedExpression declared = checkLetInitializer(*let);
        declareVariable(*let, declared.type, let->typeName.has_value());
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

void TypeChecker::recordReturn(const Token& keyword, TypeInfo type)
{
    if (returnContexts_.empty()) {
        throw TypeError("return context stack is empty");
    }

    FunctionReturnContext& context = returnContexts_.back();
    if (context.expectedReturnType && !compatible(*context.expectedReturnType, type)) {
        throw TypeError(keyword, "cannot return " + typeInfoName(type)
            + " from function returning " + typeInfoName(*context.expectedReturnType));
    }
    if (!context.expectedReturnType && type.kind == StaticType::Function) {
        type = functionWithoutSignature();
    }

    if (!context.sawReturn) {
        context.sawReturn = true;
        context.returnType = type;
        return;
    }

    context.returnType = mergeReturnTypes(context.returnType, type);
}

bool TypeChecker::bodyMayFallThrough(const std::vector<StmtPtr>& body) const
{
    return body.empty() || dynamic_cast<const ReturnStmt*>(body.back().get()) == nullptr;
}

void TypeChecker::checkImplicitNilReturn(
    const Token& functionToken,
    const std::string& functionLabel,
    const TypeInfo& expectedReturnType) const
{
    if (!compatible(expectedReturnType, simpleType(StaticType::Nil))) {
        throw TypeError(functionToken,
            "function `" + functionLabel + "` may return nil but is annotated " + typeInfoName(expectedReturnType));
    }
}

TypeInfo TypeChecker::checkFunctionBody(
    const std::vector<StmtPtr>& body,
    std::optional<TypeInfo> expectedReturnType,
    const Token& functionToken,
    const std::string& functionLabel)
{
    returnContexts_.push_back(FunctionReturnContext{false, simpleType(StaticType::Nil), expectedReturnType});

    for (const auto& child : body) {
        checkStatement(*child);
    }

    const FunctionReturnContext context = returnContexts_.back();
    returnContexts_.pop_back();

    if (expectedReturnType) {
        if (bodyMayFallThrough(body)) {
            checkImplicitNilReturn(functionToken, functionLabel, *expectedReturnType);
        }
        return *expectedReturnType;
    }

    return context.sawReturn ? context.returnType : simpleType(StaticType::Nil);
}

void TypeChecker::checkFunction(const FunctionStmt& statement)
{
    std::vector<TypeInfo> declaredParameterTypes;
    declaredParameterTypes.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        declaredParameterTypes.push_back(parameter.typeName
            ? resolveAnnotation(*parameter.typeName)
            : unknownType());
    }

    std::optional<TypeInfo> expectedReturnType;
    if (statement.returnTypeName) {
        expectedReturnType = resolveAnnotation(*statement.returnTypeName);
    }

    Binding functionBinding = declareVariable(
        statement.name,
        functionType(declaredParameterTypes, expectedReturnType ? *expectedReturnType : unknownType()),
        statement.returnTypeName.has_value());
    resolvedNames_.recordFunction(statement, functionBinding.resolvedName);

    beginScope();
    ++functionDepth_;
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;

    std::vector<std::string> parameterNames;
    for (std::size_t i = 0; i < statement.parameters.size(); ++i) {
        const Parameter& parameter = statement.parameters[i];
        Binding parameterBinding = declareVariable(parameter.name, declaredParameterTypes[i], parameter.typeName.has_value());
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(statement, std::move(parameterNames));

    const TypeInfo returnType = checkFunctionBody(
        statement.body,
        expectedReturnType,
        statement.name,
        statement.name.lexeme);

    loopDepth_ = enclosingLoopDepth;
    --functionDepth_;
    endScope();

    Binding* storedFunction = findVariable(statement.name.lexeme);
    if (!storedFunction) {
        throw TypeError(statement.name, "undefined function `" + statement.name.lexeme + "`");
    }
    storedFunction->type = functionType(std::move(declaredParameterTypes), returnType);
}

TypeChecker::CheckedExpression TypeChecker::checkFunctionExpression(const FunctionExpr& expression)
{
    resolvedNames_.recordFunction(expression, "<lambda>");

    std::vector<TypeInfo> declaredParameterTypes;
    declaredParameterTypes.reserve(expression.parameters.size());
    for (const Parameter& parameter : expression.parameters) {
        declaredParameterTypes.push_back(parameter.typeName
            ? resolveAnnotation(*parameter.typeName)
            : unknownType());
    }

    std::optional<TypeInfo> expectedReturnType;
    if (expression.returnTypeName) {
        expectedReturnType = resolveAnnotation(*expression.returnTypeName);
    }

    beginScope();
    ++functionDepth_;
    const std::size_t enclosingLoopDepth = loopDepth_;
    loopDepth_ = 0;

    std::vector<std::string> parameterNames;
    for (std::size_t i = 0; i < expression.parameters.size(); ++i) {
        const Parameter& parameter = expression.parameters[i];
        Binding parameterBinding = declareVariable(parameter.name, declaredParameterTypes[i], parameter.typeName.has_value());
        parameterNames.push_back(parameterBinding.resolvedName);
    }
    resolvedNames_.recordParameters(expression, std::move(parameterNames));

    const TypeInfo returnType = checkFunctionBody(
        expression.body,
        expectedReturnType,
        expression.keyword,
        "<lambda>");

    loopDepth_ = enclosingLoopDepth;
    --functionDepth_;
    endScope();

    return CheckedExpression{functionType(std::move(declaredParameterTypes), returnType)};
}

TypeChecker::CheckedExpression TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const CheckedExpression initializer = checkExpressionInfo(*statement.initializer);
    if (!statement.typeName) {
        return initializer;
    }

    const TypeInfo declared = resolveAnnotation(*statement.typeName);
    checkAssignable(
        statement.name,
        "cannot initialize `" + statement.name.lexeme + "` of type " + typeInfoName(declared)
            + " with " + typeInfoName(initializer.type),
        declared,
        initializer.type);
    return CheckedExpression{declared};
}

TypeInfo TypeChecker::checkExpression(const Expr& expression)
{
    return checkExpressionInfo(expression).type;
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        if (literal->value == "nil") {
            return CheckedExpression{simpleType(StaticType::Nil)};
        }
        if (literal->value == "true" || literal->value == "false") {
            return CheckedExpression{simpleType(StaticType::Bool)};
        }
        if (literal->value.size() >= 2 && literal->value.front() == '"' && literal->value.back() == '"') {
            return CheckedExpression{simpleType(StaticType::String)};
        }
        return CheckedExpression{simpleType(StaticType::Number)};
    }

    if (const auto* function = dynamic_cast<const FunctionExpr*>(&expression)) {
        return checkFunctionExpression(*function);
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        const Binding* binding = findVariable(variable->name.lexeme);
        if (!binding) {
            throw TypeError(variable->name, "undefined variable `" + variable->name.lexeme + "`");
        }
        resolvedNames_.recordVariable(*variable, binding->resolvedName);
        return CheckedExpression{binding->type};
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const CheckedExpression value = checkExpressionInfo(*assign->value);
        Binding* target = findVariable(assign->name.lexeme);
        if (!target) {
            throw TypeError(assign->name, "undefined variable `" + assign->name.lexeme + "`");
        }

        if (target->type.kind == StaticType::Function && value.type.kind == StaticType::Function) {
            if (hasFunctionSignature(target->type) && hasFunctionSignature(value.type)
                && target->type.parameterTypes.size() != value.type.parameterTypes.size()) {
                throw TypeError(assign->name,
                    "cannot assign function with " + std::to_string(value.type.parameterTypes.size())
                        + " parameters to `" + assign->name.lexeme
                        + "` of type function with " + std::to_string(target->type.parameterTypes.size()) + " parameters");
            }

            if (target->explicitType && !compatible(target->type, value.type)) {
                throw TypeError(assign->name, "cannot assign " + typeInfoName(value.type) + " to `" + assign->name.lexeme
                    + "` of type " + typeInfoName(target->type));
            }

            if (!target->explicitType) {
                target->type = value.type;
            }
        } else if (!compatible(target->type, value.type)) {
            const std::string targetTypeName = target->type.kind == StaticType::Function && !target->explicitType
                ? staticTypeName(StaticType::Function)
                : typeInfoName(target->type);
            throw TypeError(assign->name, "cannot assign " + typeInfoName(value.type) + " to `" + assign->name.lexeme
                + "` of type " + targetTypeName);
        } else if (!isKnown(target->type)) {
            target->type = value.type;
        }

        resolvedNames_.recordAssignment(*assign, target->resolvedName);
        return CheckedExpression{target->type};
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        return checkExpressionInfo(*grouping->expression);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        return CheckedExpression{checkUnary(*unary)};
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        return CheckedExpression{checkBinary(*binary)};
    }

    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&expression)) {
        const TypeInfo left = checkExpression(*logical->left);
        const TypeInfo right = checkExpression(*logical->right);
        return CheckedExpression{logicalResultType(left, right)};
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return checkCall(*call);
    }

    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        for (const auto& element : array->elements) {
            checkExpression(*element);
        }
        return CheckedExpression{simpleType(StaticType::Array)};
    }

    if (const auto* structExpr = dynamic_cast<const StructExpr*>(&expression)) {
        for (const StructField& field : structExpr->fields) {
            checkExpression(*field.value);
        }
        return CheckedExpression{simpleType(StaticType::Struct)};
    }

    if (const auto* field = dynamic_cast<const FieldAccessExpr*>(&expression)) {
        const TypeInfo object = checkExpression(*field->object);
        if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
            throw TypeError(field->name, "can only access fields on structs");
        }
        return CheckedExpression{unknownType()};
    }

    if (const auto* index = dynamic_cast<const IndexExpr*>(&expression)) {
        return CheckedExpression{checkIndex(*index)};
    }

    if (const auto* indexAssign = dynamic_cast<const IndexAssignExpr*>(&expression)) {
        return checkIndexAssignment(*indexAssign);
    }

    throw TypeError("unsupported expression node");
}

bool TypeChecker::isBuiltinLenCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && variable->name.lexeme == "len" && findVariable("len") == nullptr;
}

TypeChecker::CheckedExpression TypeChecker::checkBuiltinLenCall(const CallExpr& expression)
{
    if (expression.arguments.size() != 1) {
        throw TypeError(expression.paren, "expected 1 arguments but got " + std::to_string(expression.arguments.size()));
    }

    const CheckedExpression argument = checkExpressionInfo(*expression.arguments.front());
    if (isKnown(argument.type) && argument.type.kind != StaticType::Array && argument.type.kind != StaticType::String) {
        throw TypeError(expression.paren, "len expects array or string, got " + typeInfoName(argument.type));
    }

    return CheckedExpression{simpleType(StaticType::Number)};
}

TypeChecker::CheckedExpression TypeChecker::checkCall(const CallExpr& expression)
{
    if (isBuiltinLenCall(expression)) {
        return checkBuiltinLenCall(expression);
    }

    const CheckedExpression callee = checkExpressionInfo(*expression.callee);
    std::vector<CheckedExpression> arguments;
    arguments.reserve(expression.arguments.size());
    for (const auto& argument : expression.arguments) {
        arguments.push_back(checkExpressionInfo(*argument));
    }

    if (callee.type.kind != StaticType::Unknown && callee.type.kind != StaticType::Function) {
        throw TypeError(expression.paren, "can only call functions");
    }

    if (callee.type.kind == StaticType::Function && hasFunctionSignature(callee.type)) {
        if (callee.type.parameterTypes.size() != expression.arguments.size()) {
            throw TypeError(expression.paren, "expected " + std::to_string(callee.type.parameterTypes.size())
                + " arguments but got " + std::to_string(expression.arguments.size()));
        }
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            const TypeInfo& expected = callee.type.parameterTypes[i];
            const TypeInfo& actual = arguments[i].type;
            if (!compatible(expected, actual)) {
                throw TypeError(expression.paren,
                    "argument " + std::to_string(i + 1) + " expects " + typeInfoName(expected)
                        + ", got " + typeInfoName(actual));
            }
        }
        return CheckedExpression{*callee.type.returnType};
    }

    return CheckedExpression{unknownType()};
}

TypeInfo TypeChecker::checkIndex(const IndexExpr& expression)
{
    const TypeInfo collection = checkExpression(*expression.collection);
    const TypeInfo index = checkExpression(*expression.index);

    if (collection.kind != StaticType::Unknown && collection.kind != StaticType::Array) {
        throw TypeError(expression.bracket, "can only index arrays");
    }

    if (index.kind != StaticType::Unknown && index.kind != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    return unknownType();
}

TypeChecker::CheckedExpression TypeChecker::checkIndexAssignment(const IndexAssignExpr& expression)
{
    const TypeInfo collection = checkExpression(*expression.collection);
    const TypeInfo index = checkExpression(*expression.index);
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (collection.kind != StaticType::Unknown && collection.kind != StaticType::Array) {
        throw TypeError(expression.bracket, "can only assign array elements");
    }

    if (index.kind != StaticType::Unknown && index.kind != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    return value;
}

TypeInfo TypeChecker::resolveAnnotation(const TypeAnnotation& typeName) const
{
    if (typeName.kind == TypeAnnotation::Kind::Function) {
        std::vector<TypeInfo> parameterTypes;
        parameterTypes.reserve(typeName.parameterTypes.size());
        for (const TypeAnnotation& parameter : typeName.parameterTypes) {
            parameterTypes.push_back(resolveAnnotation(parameter));
        }
        return functionType(std::move(parameterTypes), resolveAnnotation(*typeName.returnType));
    }

    if (typeName.token.lexeme == "number") {
        return simpleType(StaticType::Number);
    }
    if (typeName.token.lexeme == "bool") {
        return simpleType(StaticType::Bool);
    }
    if (typeName.token.lexeme == "string") {
        return simpleType(StaticType::String);
    }
    if (typeName.token.lexeme == "nil") {
        return simpleType(StaticType::Nil);
    }
    throw TypeError(typeName.token, "unknown type `" + typeName.token.lexeme + "`");
}

void TypeChecker::checkAssignable(const Token& token, const std::string& context, const TypeInfo& expected, const TypeInfo& actual) const
{
    if (!compatible(expected, actual)) {
        throw TypeError(token, context);
    }
}

TypeInfo TypeChecker::checkUnary(const UnaryExpr& expression)
{
    const TypeInfo right = checkExpression(*expression.right);
    switch (expression.op.type) {
    case TokenType::Minus:
        if (isKnown(right) && right.kind != StaticType::Number) {
            throw TypeError(expression.op, "unary `-` expects number, got " + typeInfoName(right));
        }
        return simpleType(StaticType::Number);
    case TokenType::Bang:
        return simpleType(StaticType::Bool);
    default:
        throw TypeError(expression.op, "unsupported unary operator `" + expression.op.lexeme + "`");
    }
}

TypeInfo TypeChecker::checkBinary(const BinaryExpr& expression)
{
    const TypeInfo left = checkExpression(*expression.left);
    const TypeInfo right = checkExpression(*expression.right);

    switch (expression.op.type) {
    case TokenType::Plus:
        if (!isKnown(left) || !isKnown(right)) {
            return unknownType();
        }
        if (left.kind == StaticType::Number && right.kind == StaticType::Number) {
            return simpleType(StaticType::Number);
        }
        if (left.kind == StaticType::String && right.kind == StaticType::String) {
            return simpleType(StaticType::String);
        }
        throw TypeError(expression.op, "binary `+` expects two numbers or two strings, got "
            + typeInfoName(left) + " and " + typeInfoName(right));
    case TokenType::Minus:
    case TokenType::Star:
    case TokenType::Slash:
        if (!isKnown(left) || !isKnown(right)) {
            return simpleType(StaticType::Number);
        }
        if (left.kind != StaticType::Number || right.kind != StaticType::Number) {
            throw TypeError(expression.op, binaryTypesMessage(expression, left, right));
        }
        return simpleType(StaticType::Number);
    case TokenType::Greater:
    case TokenType::GreaterEqual:
    case TokenType::Less:
    case TokenType::LessEqual:
        if (!isKnown(left) || !isKnown(right)) {
            return simpleType(StaticType::Bool);
        }
        if (left.kind != StaticType::Number || right.kind != StaticType::Number) {
            throw TypeError(expression.op, binaryTypesMessage(expression, left, right));
        }
        return simpleType(StaticType::Bool);
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
        return simpleType(StaticType::Bool);
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
