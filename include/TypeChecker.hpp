#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "FlowFacts.hpp"
#include "ModuleSymbols.hpp"
#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    const std::string& methodName(const MethodDecl& method) const;
    const std::vector<std::string>& methodParameterNames(const MethodDecl& method) const;
    bool hasVariable(const VariableExpr& expression) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;
    const std::string& compoundAssignmentName(const CompoundAssignExpr& expression) const;
    const std::string& forInVariableName(const ForInStmt& statement) const;
    bool hasFieldAccess(const FieldAccessExpr& expression) const;
    const std::string& fieldAccessName(const FieldAccessExpr& expression) const;
    bool hasMemberCallCallee(const MemberCallExpr& expression) const;
    const std::string& memberCallCalleeName(const MemberCallExpr& expression) const;
    bool memberCallPassesReceiver(const MemberCallExpr& expression) const;

private:
    friend class TypeChecker;

    void clear();
    void recordLet(const LetStmt& statement, std::string name);
    void recordFunction(const FunctionStmt& statement, std::string name);
    void recordParameters(const FunctionStmt& statement, std::vector<std::string> names);
    void recordFunction(const FunctionExpr& expression, std::string name);
    void recordParameters(const FunctionExpr& expression, std::vector<std::string> names);
    void recordMethod(const MethodDecl& method, std::string name);
    void recordMethodParameters(const MethodDecl& method, std::vector<std::string> names);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);
    void recordCompoundAssignment(const CompoundAssignExpr& expression, std::string name);
    void recordForInVariable(const ForInStmt& statement, std::string name);
    void recordFieldAccess(const FieldAccessExpr& expression, std::string name);
    void recordMemberCallCallee(const MemberCallExpr& expression, std::string name, bool passesReceiver);

    std::unordered_map<const LetStmt*, std::string> letNames_;
    std::unordered_map<const FunctionStmt*, std::string> functionNames_;
    std::unordered_map<const FunctionStmt*, std::vector<std::string>> parameterNames_;
    std::unordered_map<const FunctionExpr*, std::string> functionExpressionNames_;
    std::unordered_map<const FunctionExpr*, std::vector<std::string>> functionExpressionParameterNames_;
    std::unordered_map<const MethodDecl*, std::string> methodNames_;
    std::unordered_map<const MethodDecl*, std::vector<std::string>> methodParameterNames_;
    std::unordered_map<const VariableExpr*, std::string> variableNames_;
    std::unordered_map<const AssignExpr*, std::string> assignmentNames_;
    std::unordered_map<const CompoundAssignExpr*, std::string> compoundAssignmentNames_;
    std::unordered_map<const ForInStmt*, std::string> forInVariableNames_;
    std::unordered_map<const FieldAccessExpr*, std::string> fieldAccessNames_;
    std::unordered_map<const MemberCallExpr*, std::string> memberCallCalleeNames_;
    std::unordered_map<const MemberCallExpr*, bool> memberCallPassesReceiver_;
};

class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);

private:
    struct CheckedExpression {
        TypeInfo type;
    };

    using Binding = TypeBinding;
    using StructFieldType = ::StructFieldType;
    using StructTypeDecl = ::StructTypeDecl;

    struct FunctionReturnContext {
        bool sawReturn = false;
        TypeInfo returnType;
        std::optional<TypeInfo> expectedReturnType;
    };

    struct MethodInfo {
        const MethodDecl* declaration = nullptr;
        TypeInfo receiverType;
        std::vector<TypeInfo> parameterTypes;
        TypeInfo returnType;
        std::string resolvedName;
    };

    struct IndexTargetTypes {
        TypeInfo collection;
        TypeInfo index;
    };

    using Scope = std::unordered_map<std::string, Binding>;
    using MethodTable = std::unordered_map<std::string, std::unordered_map<std::string, MethodInfo>>;

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
    Binding declareImportedVariable(const Token& name, const Binding& importedBinding);
    std::string makeResolvedName(const std::string& sourceName);
    const NamespaceImport* findNamespace(const std::string& alias) const;
    void declareNamespaceAlias(const ImportStmt& statement, NamespaceImport imported);
    std::string qualifiedStructName(const Token& qualifier, const Token& name) const;
    std::string structConstructorTypeName(const StructConstructExpr& expression) const;

    void checkStatement(const Stmt& statement);
    void checkModule(const ModuleStmt& module);
    void checkImport(const ImportStmt& statement);
    void checkExport(const ExportStmt& statement);
    std::string sourcePathLabel(const Token& path) const;
    void ensureExportNameAvailable(std::size_t moduleId, const Token& name) const;
    void forwardStructMethodExports(std::size_t targetModuleId, std::size_t currentModuleId, const std::string& structName);
    void checkReExport(const ExportStmt& statement);
    const ModuleStmt* findModule(const Program& program, std::size_t moduleId) const;
    void checkStructDeclaration(const StructDeclStmt& statement);
    void checkImpl(const ImplStmt& statement);
    std::vector<TypeInfo> resolveParameterTypes(const std::vector<Parameter>& parameters);
    std::optional<TypeInfo> resolveOptionalReturnType(const std::optional<TypeAnnotation>& returnTypeName);
    void checkMethodNameAvailable(const StructTypeDecl& structType, const ImplStmt& statement, const MethodDecl& method) const;
    void registerMethodSignature(const StructTypeDecl& structType, const ImplStmt& statement, const MethodDecl& method);
    void checkMethodBody(const std::string& structName, const MethodInfo& method);
    void checkMethodArguments(const MemberCallExpr& expression, const MethodInfo& method);
    CheckedExpression checkStructMethodCall(const MemberCallExpr& expression, const TypeInfo& receiverType);
    const MethodInfo* findMethod(const std::string& structName, const std::string& methodName) const;
    MethodSignature methodSignatureFromInfo(const MethodInfo& method) const;
    MethodInfo methodInfoFromSignature(const MethodSignature& signature) const;
    TypeInfo qualifyNamespaceType(const TypeInfo& type, const std::string& alias, const ModuleStructExports& structs) const;
    MethodSignature qualifyNamespaceMethodSignature(const MethodSignature& signature, const std::string& alias, const ModuleStructExports& structs) const;
    void importMethodExports(
        const Token& diagnosticToken,
        const ModuleMethodExports& methodExports,
        const std::string* namespaceAlias = nullptr,
        const ModuleStructExports* namespaceStructs = nullptr);
    void recordStructMethodExports(std::size_t moduleId, const std::string& structName);
    bool isBuiltinMemberName(const std::string& name) const;
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
    CheckedExpression checkExpressionInfo(const Expr& expression, const TypeInfo* expectedType);
    TypeInfo variableType(const Binding& binding) const;
    std::optional<FlowNarrowing> nonNilNarrowingForVariable(const VariableExpr& variable) const;
    CheckedExpression checkArrayLiteral(const ArrayExpr& expression, const TypeInfo* expectedType);
    TypeInfo inferArrayElementType(const ArrayExpr& expression);
    const TypeInfo* contextualFunctionType(const TypeInfo* expectedType) const;
    CheckedExpression checkFunctionExpression(const FunctionExpr& expression, const TypeInfo* expectedType);
    CheckedExpression checkCall(const CallExpr& expression);
    CheckedExpression checkMemberCall(const MemberCallExpr& expression);
    bool isBuiltinLenCall(const CallExpr& expression) const;
    CheckedExpression checkBuiltinLenCall(const CallExpr& expression);
    bool isNativeStdlibCall(const CallExpr& expression) const;
    CheckedExpression checkNativeStdlibCall(const CallExpr& expression);
    IndexTargetTypes checkIndexTarget(
        const Expr& collection,
        const Expr& index,
        const Token& bracket,
        const std::string& nonArrayMessage);
    TypeInfo checkIndex(const IndexExpr& expression);
    CheckedExpression checkIndexAssignment(const IndexAssignExpr& expression);
    CheckedExpression checkIndexCompoundAssignment(const IndexCompoundAssignExpr& expression);
    const StructFieldType* checkStructFieldTarget(
        const Expr& object,
        const Token& name,
        const std::string& nonStructMessage);
    CheckedExpression checkFieldAssignment(const FieldAssignExpr& expression);
    CheckedExpression checkFieldCompoundAssignment(const FieldCompoundAssignExpr& expression);
    void checkKnownNumber(const Token& token, const TypeInfo& type, const std::string& messagePrefix) const;
    CheckedExpression checkNamedStructFields(
        const Token& diagnosticToken,
        const TypeInfo& declared,
        const std::vector<StructField>& fields);
    CheckedExpression checkNamedStructLiteralInitializer(
        const LetStmt& statement,
        const TypeInfo& declared,
        const StructExpr& initializer);
    CheckedExpression checkStructConstructor(const StructConstructExpr& expression);
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
    MethodTable methods_;
    ModuleSymbols moduleSymbols_;
    std::unordered_set<std::size_t> checkedModules_;
    std::vector<std::size_t> moduleStack_;
    ResolvedNames resolvedNames_;
    const Program* currentProgram_ = nullptr;
    std::size_t nextResolvedName_ = 0;
    std::size_t functionDepth_ = 0;
    std::size_t loopDepth_ = 0;
    std::vector<FunctionReturnContext> returnContexts_;
    FlowFacts flowFacts_;
};
