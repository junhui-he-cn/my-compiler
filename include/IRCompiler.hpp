#pragma once

#include "Ast.hpp"
#include "IR.hpp"

#include <stdexcept>
#include <string>

class IRCompileError final : public std::runtime_error {
public:
    explicit IRCompileError(const std::string& message);
};

class IRCompiler {
public:
    IRProgram compile(const Program& program);

private:
    void compileStatement(const Stmt& statement);
    IRRegister compileExpression(const Expr& expression);
    IRRegister emitUnary(TokenType op, IRRegister value);
    IRRegister emitBinary(TokenType op, IRRegister left, IRRegister right);

    IRProgram ir_;
};
