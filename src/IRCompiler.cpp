#include "IRCompiler.hpp"

#include <cstdlib>
#include <utility>

namespace {

Value literalValue(const std::string& text)
{
    if (text == "nil") {
        return Value::nil();
    }
    if (text == "true") {
        return Value::boolean(true);
    }
    if (text == "false") {
        return Value::boolean(false);
    }
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return Value::string(text.substr(1, text.size() - 2));
    }

    std::size_t parsed = 0;
    const double number = std::stod(text, &parsed);
    if (parsed != text.size()) {
        throw IRCompileError("invalid literal: " + text);
    }
    return Value::number(number);
}

} // namespace

IRCompileError::IRCompileError(const std::string& message)
    : std::runtime_error("IR compile error: " + message)
{
}

IRProgram IRCompiler::compile(const Program& program)
{
    ir_ = IRProgram();
    for (const auto& statement : program.statements) {
        compileStatement(*statement);
    }
    return std::move(ir_);
}

void IRCompiler::compileStatement(const Stmt& statement)
{
    if (const auto* block = dynamic_cast<const BlockStmt*>(&statement)) {
        for (const auto& child : block->statements) {
            compileStatement(*child);
        }
        return;
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&statement)) {
        const IRRegister condition = compileExpression(*ifStmt->condition);
        const std::size_t jumpIfFalse = ir_.emitJumpIfFalse(condition);

        compileStatement(*ifStmt->thenBranch);

        if (ifStmt->elseBranch) {
            const std::size_t jumpOverElse = ir_.emitJump();
            ir_.patchJump(jumpIfFalse);
            compileStatement(*ifStmt->elseBranch);
            ir_.patchJump(jumpOverElse);
        } else {
            ir_.patchJump(jumpIfFalse);
        }
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const IRRegister value = compileExpression(*let->initializer);
        ir_.emitStoreVar(let->name.lexeme, value);
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        const IRRegister value = compileExpression(*print->expression);
        ir_.emitPrint(value);
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        compileExpression(*expression->expression);
        return;
    }

    throw IRCompileError("unsupported statement node");
}

IRRegister IRCompiler::compileExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return ir_.emitConstant(literalValue(literal->value));
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        return ir_.emitLoadVar(variable->name.lexeme);
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const IRRegister value = compileExpression(*assign->value);
        ir_.emitAssignVar(assign->name.lexeme, value);
        return value;
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        return compileExpression(*grouping->expression);
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        const IRRegister value = compileExpression(*unary->right);
        return emitUnary(unary->op.type, value);
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        const IRRegister left = compileExpression(*binary->left);
        const IRRegister right = compileExpression(*binary->right);
        return emitBinary(binary->op.type, left, right);
    }

    throw IRCompileError("unsupported expression node");
}

IRRegister IRCompiler::emitUnary(TokenType op, IRRegister value)
{
    switch (op) {
    case TokenType::Bang:
        return ir_.emitUnary(IROp::Not, value);
    case TokenType::Minus:
        return ir_.emitUnary(IROp::Negate, value);
    default:
        throw IRCompileError("unsupported unary operator: " + tokenTypeName(op));
    }
}

IRRegister IRCompiler::emitBinary(TokenType op, IRRegister left, IRRegister right)
{
    switch (op) {
    case TokenType::Plus:
        return ir_.emitBinary(IROp::Add, left, right);
    case TokenType::Minus:
        return ir_.emitBinary(IROp::Subtract, left, right);
    case TokenType::Star:
        return ir_.emitBinary(IROp::Multiply, left, right);
    case TokenType::Slash:
        return ir_.emitBinary(IROp::Divide, left, right);
    case TokenType::EqualEqual:
        return ir_.emitBinary(IROp::Equal, left, right);
    case TokenType::BangEqual:
        return ir_.emitBinary(IROp::NotEqual, left, right);
    case TokenType::Greater:
        return ir_.emitBinary(IROp::Greater, left, right);
    case TokenType::GreaterEqual:
        return ir_.emitBinary(IROp::GreaterEqual, left, right);
    case TokenType::Less:
        return ir_.emitBinary(IROp::Less, left, right);
    case TokenType::LessEqual:
        return ir_.emitBinary(IROp::LessEqual, left, right);
    default:
        throw IRCompileError("unsupported binary operator: " + tokenTypeName(op));
    }
}
