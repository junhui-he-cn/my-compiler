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
    stack_.clear();
    globals_.clear();

    const auto& instructions = program.instructions();
    for (std::size_t ip = 0; ip < instructions.size(); ++ip) {
        const IRInstruction& instruction = instructions[ip];
        switch (instruction.op) {
        case IROp::Constant:
            push(readConstant(program, instruction.operand));
            break;
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            const auto found = globals_.find(name);
            if (found == globals_.end()) {
                throw IRRuntimeError("undefined variable `" + name + "`");
            }
            push(found->second);
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            globals_.insert_or_assign(name, pop());
            break;
        }
        case IROp::Pop:
            pop();
            break;
        case IROp::Print:
            output_ << valueToString(pop()) << '\n';
            break;
        case IROp::Negate:
            executeUnaryNumber("negate", [](double value) {
                return makeNumber(-value);
            });
            break;
        case IROp::Not:
            push(Value::boolean(!isTruthy(pop())));
            break;
        case IROp::Add:
            executeAdd();
            break;
        case IROp::Subtract:
            executeBinaryNumber("subtract", subtractNumber);
            break;
        case IROp::Multiply:
            executeBinaryNumber("multiply", multiplyNumber);
            break;
        case IROp::Divide:
            executeBinaryNumber("divide", divideNumber);
            break;
        case IROp::Equal: {
            const Value right = pop();
            const Value left = pop();
            push(Value::boolean(valuesEqual(left, right)));
            break;
        }
        case IROp::NotEqual: {
            const Value right = pop();
            const Value left = pop();
            push(Value::boolean(!valuesEqual(left, right)));
            break;
        }
        case IROp::Greater:
            executeBinaryComparison("greater", greaterNumber);
            break;
        case IROp::GreaterEqual:
            executeBinaryComparison("greater_equal", greaterEqualNumber);
            break;
        case IROp::Less:
            executeBinaryComparison("less", lessNumber);
            break;
        case IROp::LessEqual:
            executeBinaryComparison("less_equal", lessEqualNumber);
            break;
        }
    }
}

const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    return globals_;
}

void IRInterpreter::push(Value value)
{
    stack_.push_back(std::move(value));
}

Value IRInterpreter::pop()
{
    if (stack_.empty()) {
        throw IRRuntimeError("stack underflow");
    }

    Value value = std::move(stack_.back());
    stack_.pop_back();
    return value;
}

const Value& IRInterpreter::peek() const
{
    if (stack_.empty()) {
        throw IRRuntimeError("stack underflow");
    }
    return stack_.back();
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

void IRInterpreter::executeUnaryNumber(const std::string& opName, Value (*operation)(double))
{
    const Value value = pop();
    if (value.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects number, got " + typeName(value.type()));
    }
    push(operation(value.asNumber()));
}

void IRInterpreter::executeBinaryNumber(const std::string& opName, Value (*operation)(double, double))
{
    const Value right = pop();
    const Value left = pop();
    if (left.type() != Value::Type::Number || right.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects numbers");
    }
    push(operation(left.asNumber(), right.asNumber()));
}

void IRInterpreter::executeBinaryComparison(const std::string& opName, Value (*operation)(double, double))
{
    executeBinaryNumber(opName, operation);
}

void IRInterpreter::executeAdd()
{
    const Value right = pop();
    const Value left = pop();

    if (left.type() == Value::Type::Number && right.type() == Value::Type::Number) {
        push(addNumber(left.asNumber(), right.asNumber()));
        return;
    }

    if (left.type() == Value::Type::String && right.type() == Value::Type::String) {
        push(Value::string(left.asString() + right.asString()));
        return;
    }

    throw IRRuntimeError("add expects two numbers or two strings");
}
