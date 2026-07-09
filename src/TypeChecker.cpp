#include "TypeChecker.hpp"

#include "NativeStdlib.hpp"

#include <stdexcept>
#include <utility>

namespace {

TypeInfo unknownType()
{
    return TypeInfo{};
}

TypeInfo simpleType(StaticType kind)
{
    TypeInfo result;
    result.kind = kind;
    return result;
}

TypeInfo namedStructType(std::string name)
{
    TypeInfo result;
    result.kind = StaticType::Struct;
    result.structName = std::move(name);
    return result;
}

TypeInfo arrayType(TypeInfo elementType)
{
    TypeInfo result;
    result.kind = StaticType::Array;
    result.elementType = std::make_shared<TypeInfo>(std::move(elementType));
    return result;
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
    TypeInfo result;
    result.kind = StaticType::Function;
    return result;
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
    if (type.kind == StaticType::Struct && type.structName) {
        return *type.structName;
    }

    if (type.kind == StaticType::Array && type.elementType) {
        return "[" + typeInfoName(*type.elementType) + "]";
    }

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
    if (expected.kind == StaticType::Struct) {
        if (expected.structName || actual.structName) {
            return expected.structName == actual.structName;
        }
        return true;
    }
    if (expected.kind == StaticType::Array) {
        if (!expected.elementType || !actual.elementType) {
            return true;
        }
        return compatible(*expected.elementType, *actual.elementType);
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

void rethrowWithModuleContext(const DiagnosticError& error, const ModuleStmt& module)
{
    if (error.location()) {
        throw FileDiagnosticError(error, DiagnosticSourceContext{module.path, module.source, false});
    }
    throw error;
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

const std::string& ResolvedNames::compoundAssignmentName(const CompoundAssignExpr& expression) const
{
    const auto found = compoundAssignmentNames_.find(&expression);
    if (found == compoundAssignmentNames_.end()) {
        throw std::logic_error("missing resolved compound assignment name");
    }
    return found->second;
}

const std::string& ResolvedNames::forInVariableName(const ForInStmt& statement) const
{
    const auto found = forInVariableNames_.find(&statement);
    if (found == forInVariableNames_.end()) {
        throw std::logic_error("missing resolved for-in variable name");
    }
    return found->second;
}

bool ResolvedNames::hasFieldAccess(const FieldAccessExpr& expression) const
{
    return fieldAccessNames_.find(&expression) != fieldAccessNames_.end();
}

const std::string& ResolvedNames::fieldAccessName(const FieldAccessExpr& expression) const
{
    const auto found = fieldAccessNames_.find(&expression);
    if (found == fieldAccessNames_.end()) {
        throw std::logic_error("missing resolved field access name");
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
    compoundAssignmentNames_.clear();
    forInVariableNames_.clear();
    fieldAccessNames_.clear();
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

void ResolvedNames::recordCompoundAssignment(const CompoundAssignExpr& expression, std::string name)
{
    compoundAssignmentNames_.emplace(&expression, std::move(name));
}

void ResolvedNames::recordForInVariable(const ForInStmt& statement, std::string name)
{
    forInVariableNames_.emplace(&statement, std::move(name));
}

void ResolvedNames::recordFieldAccess(const FieldAccessExpr& expression, std::string name)
{
    fieldAccessNames_.emplace(&expression, std::move(name));
}

const ResolvedNames& TypeChecker::check(const Program& program)
{
    scopes_.clear();
    structTypes_.clear();
    moduleExports_.clear();
    moduleStructExports_.clear();
    moduleLocalStructNames_.clear();
    moduleNamespaces_.clear();
    moduleImportedModules_.clear();
    checkedModules_.clear();
    moduleStack_.clear();
    resolvedNames_.clear();
    currentProgram_ = &program;
    nextResolvedName_ = 0;
    functionDepth_ = 0;
    loopDepth_ = 0;
    returnContexts_.clear();

    bool hasModules = false;
    for (const auto& statement : program.statements) {
        if (dynamic_cast<const ModuleStmt*>(statement.get())) {
            hasModules = true;
            break;
        }
    }

    if (hasModules) {
        for (const auto& statement : program.statements) {
            if (const auto* module = dynamic_cast<const ModuleStmt*>(statement.get())) {
                checkModule(*module);
            } else {
                checkStatement(*statement);
            }
        }
    } else {
        beginScope();
        for (const auto& statement : program.statements) {
            checkStatement(*statement);
        }
        endScope();
    }

    currentProgram_ = nullptr;
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

    Binding binding{std::move(type), makeResolvedName(name.lexeme), scopes_.size() - 1, functionDepth_, explicitType, false};
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

TypeChecker::Binding TypeChecker::declareImportedVariable(const Token& name, const Binding& importedBinding)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }

    Binding binding = importedBinding;
    binding.imported = true;
    binding.scopeDepth = scopes_.size() - 1;
    scope.emplace(name.lexeme, binding);
    return binding;
}

std::string TypeChecker::makeResolvedName(const std::string& sourceName)
{
    return sourceName + "#" + std::to_string(nextResolvedName_++);
}

void TypeChecker::checkStatement(const Stmt& statement)
{
    if (const auto* module = dynamic_cast<const ModuleStmt*>(&statement)) {
        checkModule(*module);
        return;
    }

    if (const auto* import = dynamic_cast<const ImportStmt*>(&statement)) {
        checkImport(*import);
        return;
    }

    if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&statement)) {
        checkExport(*exportStmt);
        return;
    }

    if (const auto* structDecl = dynamic_cast<const StructDeclStmt*>(&statement)) {
        checkStructDeclaration(*structDecl);
        return;
    }

    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        checkFunction(*function);
        return;
    }

    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (functionDepth_ == 0) {
            throw TypeError(returnStmt->keyword, "return outside function");
        }
        const TypeInfo* expectedReturn = nullptr;
        if (!returnContexts_.empty() && returnContexts_.back().expectedReturnType) {
            expectedReturn = &*returnContexts_.back().expectedReturnType;
        }
        const TypeInfo returned = returnStmt->value
            ? checkExpressionInfo(*returnStmt->value, expectedReturn).type
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

    if (const auto* forStmt = dynamic_cast<const ForStmt*>(&statement)) {
        beginScope();
        if (forStmt->initializer) {
            checkStatement(*forStmt->initializer);
        }
        if (forStmt->condition) {
            checkExpression(*forStmt->condition);
        }
        if (forStmt->increment) {
            checkExpression(*forStmt->increment);
        }
        ++loopDepth_;
        checkStatement(*forStmt->body);
        --loopDepth_;
        endScope();
        return;
    }

    if (const auto* forInStmt = dynamic_cast<const ForInStmt*>(&statement)) {
        const TypeInfo iterableType = checkExpression(*forInStmt->iterable);
        if (iterableType.kind != StaticType::Unknown && iterableType.kind != StaticType::Array) {
            throw TypeError(forInStmt->variable,
                "for-in expects array, got " + typeInfoName(iterableType));
        }

        TypeInfo elementType = unknownType();
        if (iterableType.kind == StaticType::Array && iterableType.elementType) {
            elementType = *iterableType.elementType;
        }

        beginScope();
        const Binding itemBinding = declareVariable(forInStmt->variable, elementType, false);
        resolvedNames_.recordForInVariable(*forInStmt, itemBinding.resolvedName);
        ++loopDepth_;
        if (const auto* body = dynamic_cast<const BlockStmt*>(forInStmt->body.get())) {
            for (const auto& bodyStatement : body->statements) {
                checkStatement(*bodyStatement);
            }
        } else {
            checkStatement(*forInStmt->body);
        }
        --loopDepth_;
        endScope();
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

const ModuleStmt* TypeChecker::findModule(const Program& program, std::size_t moduleId) const
{
    for (const auto& statement : program.statements) {
        if (const auto* module = dynamic_cast<const ModuleStmt*>(statement.get())) {
            if (module->moduleId == moduleId) {
                return module;
            }
        }
    }
    return nullptr;
}

void TypeChecker::checkModule(const ModuleStmt& module)
{
    if (checkedModules_.find(module.moduleId) != checkedModules_.end()) {
        return;
    }

    std::vector<Scope> savedScopes = std::move(scopes_);
    std::unordered_map<std::string, StructTypeDecl> savedStructTypes = std::move(structTypes_);
    const std::size_t savedFunctionDepth = functionDepth_;
    const std::size_t savedLoopDepth = loopDepth_;
    std::vector<FunctionReturnContext> savedReturnContexts = std::move(returnContexts_);

    scopes_.clear();
    structTypes_.clear();
    functionDepth_ = 0;
    loopDepth_ = 0;
    returnContexts_.clear();

    moduleStack_.push_back(module.moduleId);
    beginScope();
    try {
        for (const auto& statement : module.statements) {
            checkStatement(*statement);
        }
    } catch (const FileDiagnosticError&) {
        throw;
    } catch (const DiagnosticError& error) {
        rethrowWithModuleContext(error, module);
    }
    endScope();
    moduleStack_.pop_back();

    checkedModules_.insert(module.moduleId);

    scopes_ = std::move(savedScopes);
    structTypes_ = std::move(savedStructTypes);
    functionDepth_ = savedFunctionDepth;
    loopDepth_ = savedLoopDepth;
    returnContexts_ = std::move(savedReturnContexts);
}

TypeChecker::NamespaceTable& TypeChecker::currentNamespaceTable()
{
    if (moduleStack_.empty()) {
        throw TypeError("namespace imports require a module context");
    }
    return moduleNamespaces_[moduleStack_.back()];
}

const TypeChecker::NamespaceImport* TypeChecker::findNamespace(const std::string& alias) const
{
    if (moduleStack_.empty()) {
        return nullptr;
    }
    const auto table = moduleNamespaces_.find(moduleStack_.back());
    if (table == moduleNamespaces_.end()) {
        return nullptr;
    }
    const auto found = table->second.find(alias);
    return found == table->second.end() ? nullptr : &found->second;
}

std::string TypeChecker::qualifiedStructName(const Token& qualifier, const Token& name) const
{
    return qualifier.lexeme + "." + name.lexeme;
}

std::string TypeChecker::structConstructorTypeName(const StructConstructExpr& expression) const
{
    if (expression.qualifier) {
        return qualifiedStructName(*expression.qualifier, expression.name);
    }
    return expression.name.lexeme;
}

void TypeChecker::declareNamespaceAlias(const ImportStmt& statement, NamespaceImport imported)
{
    if (!statement.alias) {
        throw TypeError(statement.keyword, "internal error: namespace import without alias");
    }

    const Token& alias = *statement.alias;
    NamespaceTable& namespaces = currentNamespaceTable();
    if (namespaces.find(alias.lexeme) != namespaces.end()
        || currentScope().find(alias.lexeme) != currentScope().end()
        || structTypes_.find(alias.lexeme) != structTypes_.end()) {
        throw TypeError(alias, "namespace alias `" + alias.lexeme + "` conflicts with an existing declaration");
    }

    for (const auto& entry : imported.structs) {
        StructTypeDecl qualified = entry.second;
        qualified.name.lexeme = alias.lexeme + "." + entry.first;
        structTypes_.emplace(qualified.name.lexeme, std::move(qualified));
    }

    namespaces.emplace(alias.lexeme, std::move(imported));
}

void TypeChecker::checkImport(const ImportStmt& statement)
{
    if (moduleStack_.empty()) {
        throw TypeError(statement.keyword, "import declarations must be loaded before parsing");
    }
    if (!currentProgram_) {
        throw TypeError(statement.keyword, "internal error: import without program");
    }

    const std::size_t currentModuleId = moduleStack_.back();
    if (!statement.alias && moduleImportedModules_[currentModuleId].find(statement.resolvedModuleId)
        != moduleImportedModules_[currentModuleId].end()) {
        return;
    }
    if (!statement.alias) {
        moduleImportedModules_[currentModuleId].insert(statement.resolvedModuleId);
    }

    const ModuleStmt* imported = findModule(*currentProgram_, statement.resolvedModuleId);
    if (!imported) {
        throw TypeError(statement.keyword, "internal error: unresolved import module");
    }
    checkModule(*imported);

    if (statement.alias) {
        NamespaceImport namespaceImport;
        const auto exports = moduleExports_.find(imported->moduleId);
        if (exports != moduleExports_.end()) {
            namespaceImport.values = exports->second;
        }
        const auto structExports = moduleStructExports_.find(imported->moduleId);
        if (structExports != moduleStructExports_.end()) {
            namespaceImport.structs = structExports->second;
        }
        declareNamespaceAlias(statement, std::move(namespaceImport));
        return;
    }

    const auto exports = moduleExports_.find(imported->moduleId);
    if (exports != moduleExports_.end()) {
        for (const auto& entry : exports->second) {
            Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
            declareImportedVariable(name, entry.second);
        }
    }

    const auto structExports = moduleStructExports_.find(imported->moduleId);
    if (structExports != moduleStructExports_.end()) {
        for (const auto& entry : structExports->second) {
            if (structTypes_.find(entry.first) != structTypes_.end()) {
                Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
                throw TypeError(name, "duplicate struct `" + entry.first + "`");
            }
            structTypes_.emplace(entry.first, entry.second);
        }
    }
}

void TypeChecker::checkExport(const ExportStmt& statement)
{
    const bool inModule = !moduleStack_.empty();
    const std::size_t moduleId = inModule ? moduleStack_.back() : 0;

    for (const auto& name : statement.names) {
        bool exported = false;

        if (Binding* binding = findVariable(name.lexeme)) {
            if (binding->scopeDepth == 0 && !binding->imported) {
                if (inModule) {
                    moduleExports_[moduleId].emplace(name.lexeme, *binding);
                }
                exported = true;
            }
        }

        if (inModule) {
            const auto localStructs = moduleLocalStructNames_.find(moduleId);
            if (localStructs != moduleLocalStructNames_.end()
                && localStructs->second.find(name.lexeme) != localStructs->second.end()) {
                if (const StructTypeDecl* structType = findStructType(name.lexeme)) {
                    moduleStructExports_[moduleId].emplace(name.lexeme, *structType);
                    exported = true;
                }
            }
        } else if (findStructType(name.lexeme)) {
            exported = true;
        }

        if (!exported) {
            throw TypeError(name, "cannot export undefined name `" + name.lexeme + "`");
        }
    }
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

const TypeChecker::StructTypeDecl* TypeChecker::findStructType(const std::string& name) const
{
    const auto found = structTypes_.find(name);
    if (found == structTypes_.end()) {
        return nullptr;
    }
    return &found->second;
}

void TypeChecker::checkStructDeclaration(const StructDeclStmt& statement)
{
    if (structTypes_.find(statement.name.lexeme) != structTypes_.end()) {
        throw TypeError(statement.name, "duplicate struct `" + statement.name.lexeme + "`");
    }

    StructTypeDecl declaration{statement.name, {}};
    std::unordered_map<std::string, Token> fieldNames;
    for (const StructFieldDecl& field : statement.fields) {
        if (fieldNames.find(field.name.lexeme) != fieldNames.end()) {
            throw TypeError(field.name,
                "duplicate field `" + field.name.lexeme + "` in struct `" + statement.name.lexeme + "`");
        }
        fieldNames.emplace(field.name.lexeme, field.name);
        declaration.fields.push_back(StructFieldType{field.name, resolveAnnotation(field.typeName)});
    }

    structTypes_.emplace(statement.name.lexeme, std::move(declaration));
    if (!moduleStack_.empty()) {
        moduleLocalStructNames_[moduleStack_.back()].insert(statement.name.lexeme);
    }
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
    if (!statement.typeName) {
        return checkExpressionInfo(*statement.initializer);
    }

    const TypeInfo declared = resolveAnnotation(*statement.typeName);
    if (declared.kind == StaticType::Struct && declared.structName) {
        if (const auto* structLiteral = dynamic_cast<const StructExpr*>(statement.initializer.get())) {
            return checkNamedStructLiteralInitializer(statement, declared, *structLiteral);
        }
    }

    const CheckedExpression initializer = checkExpressionInfo(*statement.initializer, &declared);
    checkAssignable(
        statement.name,
        "cannot initialize `" + statement.name.lexeme + "` of type " + typeInfoName(declared)
            + " with " + typeInfoName(initializer.type),
        declared,
        initializer.type);
    return CheckedExpression{declared};
}

const TypeChecker::StructFieldType* TypeChecker::findStructField(
    const StructTypeDecl& structType,
    const std::string& name) const
{
    for (const StructFieldType& field : structType.fields) {
        if (field.name.lexeme == name) {
            return &field;
        }
    }
    return nullptr;
}

TypeChecker::CheckedExpression TypeChecker::checkNamedStructFields(
    const Token& diagnosticToken,
    const TypeInfo& declared,
    const std::vector<StructField>& fields)
{
    const StructTypeDecl* structType = declared.structName ? findStructType(*declared.structName) : nullptr;
    if (!structType) {
        throw TypeError(diagnosticToken, "unknown struct type `" + typeInfoName(declared) + "`");
    }

    std::unordered_map<std::string, const StructField*> literalFields;
    for (const StructField& field : fields) {
        if (literalFields.find(field.name.lexeme) != literalFields.end()) {
            throw TypeError(field.name, "duplicate field `" + field.name.lexeme + "` in struct literal");
        }
        literalFields.emplace(field.name.lexeme, &field);
    }

    for (const StructFieldType& expectedField : structType->fields) {
        const auto found = literalFields.find(expectedField.name.lexeme);
        if (found == literalFields.end()) {
            throw TypeError(diagnosticToken,
                "missing field `" + expectedField.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
        const CheckedExpression actual = checkExpressionInfo(*found->second->value, &expectedField.type);
        if (!compatible(expectedField.type, actual.type)) {
            throw TypeError(found->second->name,
                "field `" + expectedField.name.lexeme + "` expects " + typeInfoName(expectedField.type)
                    + ", got " + typeInfoName(actual.type));
        }
    }

    for (const StructField& field : fields) {
        if (!findStructField(*structType, field.name.lexeme)) {
            throw TypeError(field.name,
                "extra field `" + field.name.lexeme + "` for struct `" + structType->name.lexeme + "`");
        }
    }

    return CheckedExpression{declared};
}

TypeChecker::CheckedExpression TypeChecker::checkNamedStructLiteralInitializer(
    const LetStmt& statement,
    const TypeInfo& declared,
    const StructExpr& initializer)
{
    return checkNamedStructFields(statement.name, declared, initializer.fields);
}

TypeChecker::CheckedExpression TypeChecker::checkStructConstructor(const StructConstructExpr& expression)
{
    if (expression.qualifier) {
        const NamespaceImport* namespaceImport = findNamespace(expression.qualifier->lexeme);
        if (!namespaceImport) {
            throw TypeError(*expression.qualifier, "unknown module namespace `" + expression.qualifier->lexeme + "`");
        }
        if (namespaceImport->structs.find(expression.name.lexeme) == namespaceImport->structs.end()) {
            throw TypeError(expression.name,
                "module namespace `" + expression.qualifier->lexeme + "` has no exported type `" + expression.name.lexeme + "`");
        }
    }

    const std::string typeName = structConstructorTypeName(expression);
    const StructTypeDecl* structType = findStructType(typeName);
    if (!structType) {
        throw TypeError(expression.name, "unknown struct type `" + typeName + "`");
    }
    return checkNamedStructFields(expression.name, namedStructType(typeName), expression.fields);
}

TypeInfo TypeChecker::checkExpression(const Expr& expression)
{
    return checkExpressionInfo(expression).type;
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression)
{
    return checkExpressionInfo(expression, nullptr);
}

TypeInfo TypeChecker::inferArrayElementType(const ArrayExpr& expression)
{
    std::optional<TypeInfo> current;
    for (const auto& element : expression.elements) {
        TypeInfo elementType = checkExpression(*element);
        if (!isKnown(elementType)) {
            return unknownType();
        }
        if (!current) {
            current = std::move(elementType);
            continue;
        }
        if (!(compatible(*current, elementType) && compatible(elementType, *current))) {
            return unknownType();
        }
    }
    return current ? *current : unknownType();
}

TypeChecker::CheckedExpression TypeChecker::checkArrayLiteral(const ArrayExpr& expression, const TypeInfo* expectedType)
{
    if (expectedType && expectedType->kind == StaticType::Array && expectedType->elementType) {
        for (const auto& element : expression.elements) {
            const CheckedExpression actual = checkExpressionInfo(*element, expectedType->elementType.get());
            if (!compatible(*expectedType->elementType, actual.type)) {
                throw TypeError(expression.bracket,
                    "array element expects " + typeInfoName(*expectedType->elementType)
                        + ", got " + typeInfoName(actual.type));
            }
        }
        return CheckedExpression{*expectedType};
    }

    const TypeInfo element = inferArrayElementType(expression);
    if (isKnown(element)) {
        return CheckedExpression{arrayType(element)};
    }
    return CheckedExpression{simpleType(StaticType::Array)};
}

TypeChecker::CheckedExpression TypeChecker::checkExpressionInfo(const Expr& expression, const TypeInfo* expectedType)
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
            if (findNamespace(variable->name.lexeme)) {
                throw TypeError(variable->name, "namespace alias `" + variable->name.lexeme + "` is not a value");
            }
            throw TypeError(variable->name, "undefined variable `" + variable->name.lexeme + "`");
        }
        resolvedNames_.recordVariable(*variable, binding->resolvedName);
        return CheckedExpression{binding->type};
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        Binding* target = findVariable(assign->name.lexeme);
        if (!target) {
            if (findNamespace(assign->name.lexeme)) {
                throw TypeError(assign->name, "cannot assign to namespace alias `" + assign->name.lexeme + "`");
            }
            throw TypeError(assign->name, "undefined variable `" + assign->name.lexeme + "`");
        }

        const CheckedExpression value = checkExpressionInfo(*assign->value, &target->type);

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

    if (const auto* compound = dynamic_cast<const CompoundAssignExpr*>(&expression)) {
        Binding* target = findVariable(compound->name.lexeme);
        if (!target) {
            if (findNamespace(compound->name.lexeme)) {
                throw TypeError(compound->name, "cannot assign to namespace alias `" + compound->name.lexeme + "`");
            }
            throw TypeError(compound->name, "undefined variable `" + compound->name.lexeme + "`");
        }

        const CheckedExpression value = checkExpressionInfo(*compound->value);
        if (target->type.kind != StaticType::Unknown && target->type.kind != StaticType::Number) {
            throw TypeError(compound->op,
                "`" + compound->op.lexeme + "` expects number variable, got " + typeInfoName(target->type));
        }
        if (value.type.kind != StaticType::Unknown && value.type.kind != StaticType::Number) {
            throw TypeError(compound->op,
                "`" + compound->op.lexeme + "` expects number value, got " + typeInfoName(value.type));
        }

        if (!isKnown(target->type)) {
            target->type = simpleType(StaticType::Number);
        }
        resolvedNames_.recordCompoundAssignment(*compound, target->resolvedName);
        return CheckedExpression{simpleType(StaticType::Number)};
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
        return checkArrayLiteral(*array, expectedType);
    }

    if (const auto* structExpr = dynamic_cast<const StructExpr*>(&expression)) {
        for (const StructField& field : structExpr->fields) {
            checkExpression(*field.value);
        }
        return CheckedExpression{simpleType(StaticType::Struct)};
    }

    if (const auto* construct = dynamic_cast<const StructConstructExpr*>(&expression)) {
        return checkStructConstructor(*construct);
    }

    if (const auto* field = dynamic_cast<const FieldAccessExpr*>(&expression)) {
        if (const auto* variable = dynamic_cast<const VariableExpr*>(field->object.get())) {
            if (const NamespaceImport* namespaceImport = findNamespace(variable->name.lexeme)) {
                const auto found = namespaceImport->values.find(field->name.lexeme);
                if (found == namespaceImport->values.end()) {
                    throw TypeError(field->name,
                        "module namespace `" + variable->name.lexeme + "` has no exported member `" + field->name.lexeme + "`");
                }
                resolvedNames_.recordFieldAccess(*field, found->second.resolvedName);
                return CheckedExpression{found->second.type};
            }
        }
        const TypeInfo object = checkExpression(*field->object);
        if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
            throw TypeError(field->name, "can only access fields on structs");
        }
        if (object.kind == StaticType::Struct && object.structName) {
            const StructTypeDecl* structType = findStructType(*object.structName);
            const StructFieldType* structField = structType ? findStructField(*structType, field->name.lexeme) : nullptr;
            if (!structField) {
                throw TypeError(field->name,
                    "struct `" + *object.structName + "` has no field `" + field->name.lexeme + "`");
            }
            return CheckedExpression{structField->type};
        }
        return CheckedExpression{unknownType()};
    }

    if (const auto* fieldAssign = dynamic_cast<const FieldAssignExpr*>(&expression)) {
        return checkFieldAssignment(*fieldAssign);
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

bool TypeChecker::isNativeStdlibCall(const CallExpr& expression) const
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    return variable && isNativeStdlibName(variable->name.lexeme) && findVariable(variable->name.lexeme) == nullptr;
}

TypeChecker::CheckedExpression TypeChecker::checkNativeStdlibCall(const CallExpr& expression)
{
    const auto* variable = dynamic_cast<const VariableExpr*>(expression.callee.get());
    if (!variable) {
        throw TypeError("native stdlib call missing variable callee");
    }

    const NativeFunctionSignature* function = findNativeStdlibFunction(variable->name.lexeme);
    if (!function) {
        throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
    }
    if (expression.arguments.size() != function->arity) {
        throw TypeError(expression.paren,
            "expected " + std::to_string(function->arity) + " arguments but got " + std::to_string(expression.arguments.size()));
    }

    switch (function->kind) {
    case NativeFunctionKind::Push: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "push expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }
        const TypeInfo* expectedElement = arrayArgument.type.elementType.get();
        const CheckedExpression valueArgument = checkExpressionInfo(*expression.arguments[1], expectedElement);
        if (expectedElement && !compatible(*expectedElement, valueArgument.type)) {
            throw TypeError(expression.paren,
                "push value expects " + typeInfoName(*expectedElement)
                    + ", got " + typeInfoName(valueArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::Nil)};
    }
    case NativeFunctionKind::Pop: {
        const CheckedExpression arrayArgument = checkExpressionInfo(*expression.arguments[0]);
        if (arrayArgument.type.kind != StaticType::Unknown && arrayArgument.type.kind != StaticType::Array) {
            throw TypeError(expression.paren,
                "pop expects array as first argument, got " + typeInfoName(arrayArgument.type));
        }
        if (arrayArgument.type.kind == StaticType::Array && arrayArgument.type.elementType) {
            return CheckedExpression{*arrayArgument.type.elementType};
        }
        return CheckedExpression{unknownType()};
    }
    case NativeFunctionKind::Floor:
    case NativeFunctionKind::Ceil:
    case NativeFunctionKind::Sqrt: {
        const CheckedExpression argument = checkExpressionInfo(*expression.arguments[0]);
        if (argument.type.kind != StaticType::Unknown && argument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                std::string(function->name) + " expects number, got " + typeInfoName(argument.type));
        }
        return CheckedExpression{simpleType(StaticType::Number)};
    }
    case NativeFunctionKind::Str:
        checkExpressionInfo(*expression.arguments[0]);
        return CheckedExpression{simpleType(StaticType::String)};
    case NativeFunctionKind::Substr: {
        const CheckedExpression stringArgument = checkExpressionInfo(*expression.arguments[0]);
        if (stringArgument.type.kind != StaticType::Unknown && stringArgument.type.kind != StaticType::String) {
            throw TypeError(expression.paren,
                "substr expects string as first argument, got " + typeInfoName(stringArgument.type));
        }
        const CheckedExpression startArgument = checkExpressionInfo(*expression.arguments[1]);
        if (startArgument.type.kind != StaticType::Unknown && startArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "substr expects number as second argument, got " + typeInfoName(startArgument.type));
        }
        const CheckedExpression lengthArgument = checkExpressionInfo(*expression.arguments[2]);
        if (lengthArgument.type.kind != StaticType::Unknown && lengthArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "substr expects number as third argument, got " + typeInfoName(lengthArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }
    case NativeFunctionKind::CharAt: {
        const CheckedExpression stringArgument = checkExpressionInfo(*expression.arguments[0]);
        if (stringArgument.type.kind != StaticType::Unknown && stringArgument.type.kind != StaticType::String) {
            throw TypeError(expression.paren,
                "charAt expects string as first argument, got " + typeInfoName(stringArgument.type));
        }
        const CheckedExpression indexArgument = checkExpressionInfo(*expression.arguments[1]);
        if (indexArgument.type.kind != StaticType::Unknown && indexArgument.type.kind != StaticType::Number) {
            throw TypeError(expression.paren,
                "charAt expects number as second argument, got " + typeInfoName(indexArgument.type));
        }
        return CheckedExpression{simpleType(StaticType::String)};
    }
    }

    throw TypeError(variable->name, "unknown native stdlib function `" + variable->name.lexeme + "`");
}

TypeChecker::CheckedExpression TypeChecker::checkCall(const CallExpr& expression)
{
    if (isBuiltinLenCall(expression)) {
        return checkBuiltinLenCall(expression);
    }

    if (isNativeStdlibCall(expression)) {
        return checkNativeStdlibCall(expression);
    }

    const CheckedExpression callee = checkExpressionInfo(*expression.callee);
    std::vector<CheckedExpression> arguments;
    arguments.reserve(expression.arguments.size());

    if (callee.type.kind == StaticType::Function && hasFunctionSignature(callee.type)) {
        if (callee.type.parameterTypes.size() != expression.arguments.size()) {
            throw TypeError(expression.paren, "expected " + std::to_string(callee.type.parameterTypes.size())
                + " arguments but got " + std::to_string(expression.arguments.size()));
        }
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            arguments.push_back(checkExpressionInfo(*expression.arguments[i], &callee.type.parameterTypes[i]));
        }
    } else {
        for (const auto& argument : expression.arguments) {
            arguments.push_back(checkExpressionInfo(*argument));
        }
    }

    if (callee.type.kind != StaticType::Unknown && callee.type.kind != StaticType::Function) {
        throw TypeError(expression.paren, "can only call functions");
    }

    if (callee.type.kind == StaticType::Function && hasFunctionSignature(callee.type)) {
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

    if (collection.kind == StaticType::Array && collection.elementType) {
        return *collection.elementType;
    }
    return unknownType();
}

TypeChecker::CheckedExpression TypeChecker::checkIndexAssignment(const IndexAssignExpr& expression)
{
    const TypeInfo collection = checkExpression(*expression.collection);
    const TypeInfo index = checkExpression(*expression.index);

    if (collection.kind != StaticType::Unknown && collection.kind != StaticType::Array) {
        throw TypeError(expression.bracket, "can only assign array elements");
    }

    if (index.kind != StaticType::Unknown && index.kind != StaticType::Number) {
        throw TypeError(expression.bracket, "array index must be number");
    }

    const TypeInfo* expectedElement = collection.elementType.get();
    const CheckedExpression value = checkExpressionInfo(*expression.value, expectedElement);
    if (expectedElement && !compatible(*expectedElement, value.type)) {
        throw TypeError(expression.bracket,
            "array index assignment expects " + typeInfoName(*expectedElement)
                + ", got " + typeInfoName(value.type));
    }

    return value;
}

TypeChecker::CheckedExpression TypeChecker::checkFieldAssignment(const FieldAssignExpr& expression)
{
    const TypeInfo object = checkExpression(*expression.object);
    const CheckedExpression value = checkExpressionInfo(*expression.value);

    if (object.kind != StaticType::Unknown && object.kind != StaticType::Struct) {
        throw TypeError(expression.name, "can only assign fields on structs");
    }

    if (object.kind == StaticType::Struct && object.structName) {
        const StructTypeDecl* structType = findStructType(*object.structName);
        const StructFieldType* structField = structType ? findStructField(*structType, expression.name.lexeme) : nullptr;
        if (!structField) {
            throw TypeError(expression.name,
                "struct `" + *object.structName + "` has no field `" + expression.name.lexeme + "`");
        }
        if (!compatible(structField->type, value.type)) {
            throw TypeError(expression.name,
                "field `" + expression.name.lexeme + "` expects " + typeInfoName(structField->type)
                    + ", got " + typeInfoName(value.type));
        }
        return CheckedExpression{structField->type};
    }

    return value;
}

TypeInfo TypeChecker::resolveAnnotation(const TypeAnnotation& typeName) const
{
    if (typeName.kind == TypeAnnotation::Kind::Array) {
        return arrayType(resolveAnnotation(*typeName.elementType));
    }

    if (typeName.kind == TypeAnnotation::Kind::Function) {
        std::vector<TypeInfo> parameterTypes;
        parameterTypes.reserve(typeName.parameterTypes.size());
        for (const TypeAnnotation& parameter : typeName.parameterTypes) {
            parameterTypes.push_back(resolveAnnotation(parameter));
        }
        return functionType(std::move(parameterTypes), resolveAnnotation(*typeName.returnType));
    }

    if (typeName.kind == TypeAnnotation::Kind::Qualified) {
        const NamespaceImport* namespaceImport = findNamespace(typeName.qualifier.lexeme);
        if (!namespaceImport) {
            throw TypeError(typeName.qualifier, "unknown module namespace `" + typeName.qualifier.lexeme + "`");
        }
        if (namespaceImport->structs.find(typeName.token.lexeme) == namespaceImport->structs.end()) {
            throw TypeError(typeName.token,
                "module namespace `" + typeName.qualifier.lexeme + "` has no exported type `" + typeName.token.lexeme + "`");
        }
        return namedStructType(qualifiedStructName(typeName.qualifier, typeName.token));
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
    if (findStructType(typeName.token.lexeme)) {
        return namedStructType(typeName.token.lexeme);
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
