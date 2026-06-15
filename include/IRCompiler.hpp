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
    void compileExpression(const Expr& expression);
    void emitUnary(TokenType op);
    void emitBinary(TokenType op);

    IRProgram ir_;
};
