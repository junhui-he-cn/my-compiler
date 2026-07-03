#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Token.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
    Function,
    Array,
};

class TypeError final : public DiagnosticError {
public:
    explicit TypeError(std::string message);
    TypeError(const Token& token, std::string message);
};

class ResolvedNames {
public:
    const std::string& letName(const LetStmt& statement) const;
    const std::string& functionName(const FunctionStmt& statement) const;
    const std::vector<std::string>& parameterNames(const FunctionStmt& statement) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;

private:
    friend class TypeChecker;

    void clear();
    void recordLet(const LetStmt& statement, std::string name);
    void recordFunction(const FunctionStmt& statement, std::string name);
    void recordParameters(const FunctionStmt& statement, std::vector<std::string> names);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);

    std::unordered_map<const LetStmt*, std::string> letNames_;
    std::unordered_map<const FunctionStmt*, std::string> functionNames_;
    std::unordered_map<const FunctionStmt*, std::vector<std::string>> parameterNames_;
    std::unordered_map<const VariableExpr*, std::string> variableNames_;
    std::unordered_map<const AssignExpr*, std::string> assignmentNames_;
};

class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);

private:
    struct Binding {
        StaticType type;
        std::string resolvedName;
        std::optional<std::size_t> arity;
        std::size_t scopeDepth = 0;
        std::size_t functionDepth = 0;
    };

    using Scope = std::unordered_map<std::string, Binding>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    Binding* findVariable(const std::string& name);
    const Binding* findVariable(const std::string& name) const;
    Binding declareVariable(const Token& name, StaticType type, std::optional<std::size_t> arity = std::nullopt);
    Binding declareVariable(const LetStmt& statement, StaticType type);
    std::string makeResolvedName(const std::string& sourceName);

    void checkStatement(const Stmt& statement);
    void checkFunction(const FunctionStmt& statement);
    StaticType checkExpression(const Expr& expression);
    StaticType checkCall(const CallExpr& expression);
    StaticType checkIndex(const IndexExpr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
    StaticType resolveAnnotation(const Token& typeName) const;
    void checkAssignable(const Token& token, const std::string& context, StaticType expected, StaticType actual) const;
    StaticType checkUnary(const UnaryExpr& expression);
    StaticType checkBinary(const BinaryExpr& expression);
    bool isGlobalBinding(const Binding& binding) const;
    bool isCurrentFunctionBinding(const Binding& binding) const;

    std::vector<Scope> scopes_;
    ResolvedNames resolvedNames_;
    std::size_t nextResolvedName_ = 0;
    std::size_t functionDepth_ = 0;
};

std::string staticTypeName(StaticType type);
