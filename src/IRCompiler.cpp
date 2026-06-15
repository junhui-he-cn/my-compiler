#include "IRCompiler.hpp"

#include <cstdlib>

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
    return ir_;
}

void IRCompiler::compileStatement(const Stmt& statement)
{
    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        if (let->initializer) {
            compileExpression(*let->initializer);
        } else {
            ir_.emit(IROp::Constant, ir_.addConstant(Value::nil()));
        }
        ir_.emit(IROp::StoreVar, ir_.addName(let->name.lexeme));
        return;
    }

    if (const auto* print = dynamic_cast<const PrintStmt*>(&statement)) {
        compileExpression(*print->expression);
        ir_.emit(IROp::Print);
        return;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStmt*>(&statement)) {
        compileExpression(*expression->expression);
        ir_.emit(IROp::Pop);
        return;
    }

    throw IRCompileError("unsupported statement node");
}

void IRCompiler::compileExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        ir_.emit(IROp::Constant, ir_.addConstant(literalValue(literal->value)));
        return;
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        ir_.emit(IROp::LoadVar, ir_.addName(variable->name.lexeme));
        return;
    }

    if (const auto* grouping = dynamic_cast<const GroupingExpr*>(&expression)) {
        compileExpression(*grouping->expression);
        return;
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        compileExpression(*unary->right);
        emitUnary(unary->op.type);
        return;
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        compileExpression(*binary->left);
        compileExpression(*binary->right);
        emitBinary(binary->op.type);
        return;
    }

    throw IRCompileError("unsupported expression node");
}

void IRCompiler::emitUnary(TokenType op)
{
    switch (op) {
    case TokenType::Bang:
        ir_.emit(IROp::Not);
        return;
    case TokenType::Minus:
        ir_.emit(IROp::Negate);
        return;
    default:
        throw IRCompileError("unsupported unary operator: " + tokenTypeName(op));
    }
}

void IRCompiler::emitBinary(TokenType op)
{
    switch (op) {
    case TokenType::Plus:
        ir_.emit(IROp::Add);
        return;
    case TokenType::Minus:
        ir_.emit(IROp::Subtract);
        return;
    case TokenType::Star:
        ir_.emit(IROp::Multiply);
        return;
    case TokenType::Slash:
        ir_.emit(IROp::Divide);
        return;
    case TokenType::EqualEqual:
        ir_.emit(IROp::Equal);
        return;
    case TokenType::BangEqual:
        ir_.emit(IROp::NotEqual);
        return;
    case TokenType::Greater:
        ir_.emit(IROp::Greater);
        return;
    case TokenType::GreaterEqual:
        ir_.emit(IROp::GreaterEqual);
        return;
    case TokenType::Less:
        ir_.emit(IROp::Less);
        return;
    case TokenType::LessEqual:
        ir_.emit(IROp::LessEqual);
        return;
    default:
        throw IRCompileError("unsupported binary operator: " + tokenTypeName(op));
    }
}
