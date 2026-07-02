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

IRCompileError::IRCompileError(std::string message)
    : DiagnosticError(DiagnosticKind::Compile, std::move(message))
{
}

IRProgram IRCompiler::compile(const Program& program, const ResolvedNames& resolvedNames)
{
    ir_ = IRProgram();
    resolvedNames_ = &resolvedNames;
    for (const auto& statement : program.statements) {
        compileStatement(*statement);
    }
    resolvedNames_ = nullptr;
    return std::move(ir_);
}

void IRCompiler::compileStatement(const Stmt& statement)
{
    if (const auto* function = dynamic_cast<const FunctionStmt*>(&statement)) {
        compileFunctionStatement(*function);
        return;
    }

    if (const auto* returnStmt = dynamic_cast<const ReturnStmt*>(&statement)) {
        compileReturn(*returnStmt);
        return;
    }

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

    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&statement)) {
        const std::size_t loopStart = ir_.instructionCount();
        const IRRegister condition = compileExpression(*whileStmt->condition);
        const std::size_t exitJump = ir_.emitJumpIfFalse(condition);
        compileStatement(*whileStmt->body);
        ir_.emitJumpTo(loopStart);
        ir_.patchJump(exitJump);
        return;
    }

    if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
        const IRRegister value = compileExpression(*let->initializer);
        ir_.emitStoreVar(resolvedNames_->letName(*let), value);
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

void IRCompiler::compileFunctionStatement(const FunctionStmt& function)
{
    std::vector<std::string> parameters = resolvedNames_->parameterNames(function);
    ir_.beginFunction(function.name.lexeme, std::move(parameters));

    for (const auto& statement : function.body) {
        compileStatement(*statement);
    }
    IRRegister nilValue = ir_.emitConstant(Value::nil());
    ir_.emitReturn(nilValue);

    const std::size_t functionIndex = ir_.endFunction();
    IRRegister value = ir_.emitMakeFunction(functionIndex);
    ir_.emitStoreVar(resolvedNames_->functionName(function), value);
}

void IRCompiler::compileReturn(const ReturnStmt& statement)
{
    IRRegister value = statement.value ? compileExpression(*statement.value) : ir_.emitConstant(Value::nil());
    ir_.emitReturn(value);
}

IRRegister IRCompiler::compileExpression(const Expr& expression)
{
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        return ir_.emitConstant(literalValue(literal->value));
    }

    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expression)) {
        return ir_.emitLoadVar(resolvedNames_->variableName(*variable));
    }

    if (const auto* assign = dynamic_cast<const AssignExpr*>(&expression)) {
        const IRRegister value = compileExpression(*assign->value);
        ir_.emitAssignVar(resolvedNames_->assignmentName(*assign), value);
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

    if (const auto* logical = dynamic_cast<const LogicalExpr*>(&expression)) {
        return emitLogical(*logical);
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        return emitCall(*call);
    }

    throw IRCompileError("unsupported expression node");
}

IRRegister IRCompiler::emitCall(const CallExpr& expression)
{
    IRRegister callee = compileExpression(*expression.callee);
    std::vector<IRRegister> arguments;
    for (const auto& argument : expression.arguments) {
        arguments.push_back(compileExpression(*argument));
    }
    return ir_.emitCall(callee, std::move(arguments));
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

IRRegister IRCompiler::emitLogical(const LogicalExpr& expression)
{
    const IRRegister left = compileExpression(*expression.left);
    const IRRegister result = ir_.emitCopy(left);

    std::size_t jump = 0;
    switch (expression.op.type) {
    case TokenType::PipePipe:
        jump = ir_.emitJumpIfTrue(result);
        break;
    case TokenType::AmpersandAmpersand:
        jump = ir_.emitJumpIfFalse(result);
        break;
    default:
        throw IRCompileError("unsupported logical operator: " + tokenTypeName(expression.op.type));
    }

    const IRRegister right = compileExpression(*expression.right);
    ir_.emitCopyTo(result, right);
    ir_.patchJump(jump);
    return result;
}
