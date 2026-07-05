#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "IR.hpp"
#include "TypeChecker.hpp"

#include <cstddef>
#include <string>
#include <vector>

class IRCompileError final : public DiagnosticError {
public:
    explicit IRCompileError(std::string message);
};

class IRCompiler {
public:
    IRProgram compile(const Program& program, const ResolvedNames& resolvedNames);

private:
    void compileStatement(const Stmt& statement);
    void compileFunctionStatement(const FunctionStmt& function);
    void compileReturn(const ReturnStmt& statement);
    IRRegister compileExpression(const Expr& expression);
    IRRegister emitCall(const CallExpr& expression);
    bool isBuiltinLenCall(const CallExpr& expression) const;
    void compileBreak(const BreakStmt& statement);
    void compileContinue(const ContinueStmt& statement);
    IRRegister emitLenCall(const CallExpr& expression);
    IRRegister emitFunctionExpr(const FunctionExpr& expression);
    IRRegister emitArray(const ArrayExpr& expression);
    IRRegister emitStruct(const StructExpr& expression);
    IRRegister emitIndex(const IndexExpr& expression);
    IRRegister emitIndexAssign(const IndexAssignExpr& expression);
    IRRegister emitFieldAccess(const FieldAccessExpr& expression);
    IRRegister emitUnary(TokenType op, IRRegister value);
    IRRegister emitBinary(TokenType op, IRRegister left, IRRegister right);
    IRRegister emitLogical(const LogicalExpr& expression);

    struct LoopContext {
        std::size_t continueTarget = 0;
        std::vector<std::size_t> breakJumps;
    };

    IRProgram ir_;
    const ResolvedNames* resolvedNames_ = nullptr;
    std::vector<LoopContext> loopContexts_;
};
