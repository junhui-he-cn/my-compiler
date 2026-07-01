#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
};

class TypeError final : public std::runtime_error {
public:
    explicit TypeError(const std::string& message);
};

class ResolvedNames {
public:
    const std::string& letName(const LetStmt& statement) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;

private:
    friend class TypeChecker;

    void clear();
    void recordLet(const LetStmt& statement, std::string name);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);

    std::unordered_map<const LetStmt*, std::string> letNames_;
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
    };

    using Scope = std::unordered_map<std::string, Binding>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    Binding* findVariable(const std::string& name);
    const Binding* findVariable(const std::string& name) const;
    Binding declareVariable(const LetStmt& statement, StaticType type);
    std::string makeResolvedName(const std::string& sourceName);

    void checkStatement(const Stmt& statement);
    StaticType checkExpression(const Expr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
    StaticType resolveAnnotation(const Token& typeName) const;
    void checkAssignable(const std::string& context, StaticType expected, StaticType actual) const;
    StaticType checkUnary(const UnaryExpr& expression);
    StaticType checkBinary(const BinaryExpr& expression);

    std::vector<Scope> scopes_;
    ResolvedNames resolvedNames_;
    std::size_t nextResolvedName_ = 0;
};

std::string staticTypeName(StaticType type);
