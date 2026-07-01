#include "IRInterpreter.hpp"

#include <cmath>
#include <utility>

namespace {

Value makeNumber(double value)
{
    return Value::number(value);
}

Value addNumber(double left, double right)
{
    return Value::number(left + right);
}

Value subtractNumber(double left, double right)
{
    return Value::number(left - right);
}

Value multiplyNumber(double left, double right)
{
    return Value::number(left * right);
}

Value divideNumber(double left, double right)
{
    if (right == 0.0) {
        throw IRRuntimeError("division by zero");
    }
    return Value::number(left / right);
}

Value greaterNumber(double left, double right)
{
    return Value::boolean(left > right);
}

Value greaterEqualNumber(double left, double right)
{
    return Value::boolean(left >= right);
}

Value lessNumber(double left, double right)
{
    return Value::boolean(left < right);
}

Value lessEqualNumber(double left, double right)
{
    return Value::boolean(left <= right);
}

std::string typeName(Value::Type type)
{
    switch (type) {
    case Value::Type::Nil:
        return "nil";
    case Value::Type::Number:
        return "number";
    case Value::Type::Bool:
        return "bool";
    case Value::Type::String:
        return "string";
    }

    return "unknown";
}

void validateJumpTarget(std::size_t target, std::size_t instructionCount)
{
    if (target > instructionCount) {
        throw IRRuntimeError("jump target out of range");
    }
}

} // namespace

IRRuntimeError::IRRuntimeError(const std::string& message)
    : std::runtime_error("IR runtime error: " + message)
{
}

IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
{
}

void IRInterpreter::execute(const IRProgram& program)
{
    registers_.assign(program.registerCount(), Value::nil());
    globals_.clear();

    const auto& instructions = program.instructions();
    std::size_t ip = 0;
    while (ip < instructions.size()) {
        const IRInstruction& instruction = instructions[ip];
        switch (instruction.op) {
        case IROp::Constant:
            writeRegister(readDest(instruction), readConstant(program, instruction.operand));
            break;
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            const auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            writeRegister(readDest(instruction), found->second);
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            globals_.insert_or_assign(name, readRegister(readLeft(instruction)));
            break;
        }
        case IROp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            found->second = readRegister(readLeft(instruction));
            break;
        }
        case IROp::Print:
            output_ << valueToString(readRegister(readLeft(instruction))) << '\n';
            break;
        case IROp::Negate:
            writeRegister(readDest(instruction), executeUnaryNumber("negate", readLeft(instruction), [](double value) {
                return makeNumber(-value);
            }));
            break;
        case IROp::Not:
            writeRegister(readDest(instruction), Value::boolean(!isTruthy(readRegister(readLeft(instruction)))));
            break;
        case IROp::Add:
            writeRegister(readDest(instruction), executeAdd(readLeft(instruction), readRight(instruction)));
            break;
        case IROp::Subtract:
            writeRegister(readDest(instruction), executeBinaryNumber("subtract", readLeft(instruction), readRight(instruction), subtractNumber));
            break;
        case IROp::Multiply:
            writeRegister(readDest(instruction), executeBinaryNumber("multiply", readLeft(instruction), readRight(instruction), multiplyNumber));
            break;
        case IROp::Divide:
            writeRegister(readDest(instruction), executeBinaryNumber("divide", readLeft(instruction), readRight(instruction), divideNumber));
            break;
        case IROp::Equal:
            writeRegister(readDest(instruction), Value::boolean(valuesEqual(readRegister(readLeft(instruction)), readRegister(readRight(instruction)))));
            break;
        case IROp::NotEqual:
            writeRegister(readDest(instruction), Value::boolean(!valuesEqual(readRegister(readLeft(instruction)), readRegister(readRight(instruction)))));
            break;
        case IROp::Greater:
            writeRegister(readDest(instruction), executeBinaryComparison("greater", readLeft(instruction), readRight(instruction), greaterNumber));
            break;
        case IROp::GreaterEqual:
            writeRegister(readDest(instruction), executeBinaryComparison("greater_equal", readLeft(instruction), readRight(instruction), greaterEqualNumber));
            break;
        case IROp::Less:
            writeRegister(readDest(instruction), executeBinaryComparison("less", readLeft(instruction), readRight(instruction), lessNumber));
            break;
        case IROp::LessEqual:
            writeRegister(readDest(instruction), executeBinaryComparison("less_equal", readLeft(instruction), readRight(instruction), lessEqualNumber));
            break;
        case IROp::Jump:
            validateJumpTarget(instruction.operand, instructions.size());
            ip = instruction.operand;
            continue;
        case IROp::JumpIfFalse:
            validateJumpTarget(instruction.operand, instructions.size());
            if (!isTruthy(readRegister(readLeft(instruction)))) {
                ip = instruction.operand;
                continue;
            }
            break;
        }

        ++ip;
    }
}

const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    return globals_;
}

Value IRInterpreter::readConstant(const IRProgram& program, std::size_t index) const
{
    if (index >= program.constants().size()) {
        throw IRRuntimeError("constant index out of range");
    }
    return program.constants()[index];
}

std::string IRInterpreter::readName(const IRProgram& program, std::size_t index) const
{
    if (index >= program.names().size()) {
        throw IRRuntimeError("name index out of range");
    }
    return program.names()[index];
}

IRRegister IRInterpreter::readDest(const IRInstruction& instruction) const
{
    if (!instruction.dest) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing destination register");
    }
    return *instruction.dest;
}

IRRegister IRInterpreter::readLeft(const IRInstruction& instruction) const
{
    if (!instruction.left) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing left register");
    }
    return *instruction.left;
}

IRRegister IRInterpreter::readRight(const IRInstruction& instruction) const
{
    if (!instruction.right) {
        throw IRRuntimeError(irOpName(instruction.op) + " missing right register");
    }
    return *instruction.right;
}

const Value& IRInterpreter::readRegister(IRRegister reg) const
{
    if (reg.index >= registers_.size()) {
        throw IRRuntimeError("register index out of range");
    }
    return registers_[reg.index];
}

void IRInterpreter::writeRegister(IRRegister reg, Value value)
{
    if (reg.index >= registers_.size()) {
        throw IRRuntimeError("register index out of range");
    }
    registers_[reg.index] = std::move(value);
}


Value IRInterpreter::executeUnaryNumber(const std::string& opName, IRRegister value, Value (*operation)(double))
{
    const Value& input = readRegister(value);
    if (input.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects number, got " + typeName(input.type()));
    }
    return operation(input.asNumber());
}

Value IRInterpreter::executeBinaryNumber(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    const Value& leftValue = readRegister(left);
    const Value& rightValue = readRegister(right);
    if (leftValue.type() != Value::Type::Number || rightValue.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects numbers");
    }
    return operation(leftValue.asNumber(), rightValue.asNumber());
}

Value IRInterpreter::executeBinaryComparison(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    return executeBinaryNumber(opName, left, right, operation);
}

Value IRInterpreter::executeAdd(IRRegister left, IRRegister right)
{
    const Value& leftValue = readRegister(left);
    const Value& rightValue = readRegister(right);

    if (leftValue.type() == Value::Type::Number && rightValue.type() == Value::Type::Number) {
        return addNumber(leftValue.asNumber(), rightValue.asNumber());
    }

    if (leftValue.type() == Value::Type::String && rightValue.type() == Value::Type::String) {
        return Value::string(leftValue.asString() + rightValue.asString());
    }

    throw IRRuntimeError("add expects two numbers or two strings");
}
