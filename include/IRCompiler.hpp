#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "IR.hpp"
#include "TypeChecker.hpp"

#include <string>

class IRCompileError final : public DiagnosticError {
public:
    explicit IRCompileError(std::string message);
};

class IRCompiler {
public:
    IRProgram compile(const Program& program, const ResolvedNames& resolvedNames);

private:
    void compileStatement(const Stmt& statement);
    IRRegister compileExpression(const Expr& expression);
    IRRegister emitUnary(TokenType op, IRRegister value);
    IRRegister emitBinary(TokenType op, IRRegister left, IRRegister right);
    IRRegister emitLogical(const LogicalExpr& expression);

    IRProgram ir_;
    const ResolvedNames* resolvedNames_ = nullptr;
};
