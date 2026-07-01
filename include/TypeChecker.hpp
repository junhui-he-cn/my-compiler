#pragma once

#include "Ast.hpp"
#include "Token.hpp"

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

class TypeChecker {
public:
    void check(const Program& program);

private:
    using Scope = std::unordered_map<std::string, StaticType>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    StaticType* findVariable(const std::string& name);
    const StaticType* findVariable(const std::string& name) const;
    void declareVariable(const Token& name, StaticType type);

    void checkStatement(const Stmt& statement);
    StaticType checkExpression(const Expr& expression);
    StaticType checkLetInitializer(const LetStmt& statement);
    StaticType resolveAnnotation(const Token& typeName) const;
    void checkAssignable(const std::string& context, StaticType expected, StaticType actual) const;
    StaticType checkUnary(const UnaryExpr& expression);
    StaticType checkBinary(const BinaryExpr& expression);

    std::vector<Scope> scopes_;
};

std::string staticTypeName(StaticType type);
