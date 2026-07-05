#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Token.hpp"

#include <cstddef>
#include <memory>
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
    Struct,
};

struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
    std::optional<std::string> structName;
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
    const std::string& functionName(const FunctionExpr& expression) const;
    const std::vector<std::string>& parameterNames(const FunctionExpr& expression) const;
    bool hasVariable(const VariableExpr& expression) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;

private:
    friend class TypeChecker;

    void clear();
    void recordLet(const LetStmt& statement, std::string name);
    void recordFunction(const FunctionStmt& statement, std::string name);
    void recordParameters(const FunctionStmt& statement, std::vector<std::string> names);
    void recordFunction(const FunctionExpr& expression, std::string name);
    void recordParameters(const FunctionExpr& expression, std::vector<std::string> names);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);

    std::unordered_map<const LetStmt*, std::string> letNames_;
    std::unordered_map<const FunctionStmt*, std::string> functionNames_;
    std::unordered_map<const FunctionStmt*, std::vector<std::string>> parameterNames_;
    std::unordered_map<const FunctionExpr*, std::string> functionExpressionNames_;
    std::unordered_map<const FunctionExpr*, std::vector<std::string>> functionExpressionParameterNames_;
    std::unordered_map<const VariableExpr*, std::string> variableNames_;
    std::unordered_map<const AssignExpr*, std::string> assignmentNames_;
};

class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);

private:
    struct CheckedExpression {
        TypeInfo type;
    };

    struct Binding {
        TypeInfo type;
        std::string resolvedName;
        std::size_t scopeDepth = 0;
        std::size_t functionDepth = 0;
        bool explicitType = false;
    };

    struct FunctionReturnContext {
        bool sawReturn = false;
        TypeInfo returnType;
        std::optional<TypeInfo> expectedReturnType;
    };

    struct StructFieldType {
        Token name;
        TypeInfo type;
    };

    struct StructTypeDecl {
        Token name;
        std::vector<StructFieldType> fields;
    };

    using Scope = std::unordered_map<std::string, Binding>;

    void beginScope();
    void endScope();
    Scope& currentScope();
    const Scope& currentScope() const;
    Binding* findVariable(const std::string& name);
    const Binding* findVariable(const std::string& name) const;
    Binding declareVariable(
        const Token& name,
        TypeInfo type,
        bool explicitType = false);
    Binding declareVariable(
        const LetStmt& statement,
        TypeInfo type,
        bool explicitType = false);
    std::string makeResolvedName(const std::string& sourceName);

    void checkStatement(const Stmt& statement);
    void checkStructDeclaration(const StructDeclStmt& statement);
    const StructTypeDecl* findStructType(const std::string& name) const;
    void checkFunction(const FunctionStmt& statement);
    TypeInfo checkFunctionBody(
        const std::vector<StmtPtr>& body,
        std::optional<TypeInfo> expectedReturnType,
        const Token& functionToken,
        const std::string& functionLabel);
    void recordReturn(const Token& keyword, TypeInfo type);
    bool bodyMayFallThrough(const std::vector<StmtPtr>& body) const;
    void checkImplicitNilReturn(const Token& functionToken, const std::string& functionLabel, const TypeInfo& expectedReturnType) const;
    TypeInfo checkExpression(const Expr& expression);
    CheckedExpression checkExpressionInfo(const Expr& expression);
    CheckedExpression checkFunctionExpression(const FunctionExpr& expression);
    CheckedExpression checkCall(const CallExpr& expression);
    bool isBuiltinLenCall(const CallExpr& expression) const;
    CheckedExpression checkBuiltinLenCall(const CallExpr& expression);
    bool isNativeStdlibCall(const CallExpr& expression) const;
    CheckedExpression checkNativeStdlibCall(const CallExpr& expression);
    TypeInfo checkIndex(const IndexExpr& expression);
    CheckedExpression checkIndexAssignment(const IndexAssignExpr& expression);
    CheckedExpression checkFieldAssignment(const FieldAssignExpr& expression);
    CheckedExpression checkNamedStructLiteralInitializer(
        const LetStmt& statement,
        const TypeInfo& declared,
        const StructExpr& initializer);
    const StructFieldType* findStructField(const StructTypeDecl& structType, const std::string& name) const;
    CheckedExpression checkLetInitializer(const LetStmt& statement);
    TypeInfo resolveAnnotation(const TypeAnnotation& typeName) const;
    void checkAssignable(const Token& token, const std::string& context, const TypeInfo& expected, const TypeInfo& actual) const;
    TypeInfo checkUnary(const UnaryExpr& expression);
    TypeInfo checkBinary(const BinaryExpr& expression);
    bool isGlobalBinding(const Binding& binding) const;
    bool isCurrentFunctionBinding(const Binding& binding) const;

    std::vector<Scope> scopes_;
    std::unordered_map<std::string, StructTypeDecl> structTypes_;
    ResolvedNames resolvedNames_;
    std::size_t nextResolvedName_ = 0;
    std::size_t functionDepth_ = 0;
    std::size_t loopDepth_ = 0;
    std::vector<FunctionReturnContext> returnContexts_;
};

std::string staticTypeName(StaticType type);
