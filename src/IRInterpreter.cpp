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
    case Value::Type::Function:
        return "function";
    case Value::Type::Array:
        return "array";
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

IRRuntimeError::IRRuntimeError(std::string message)
    : DiagnosticError(DiagnosticKind::Runtime, std::move(message))
{
}

IRInterpreter::IRInterpreter(std::ostream& output)
    : output_(output)
{
}

void IRInterpreter::execute(const IRProgram& program)
{
    globals_ = std::make_shared<Environment>();
    globalsView_.clear();
    nextFunctionIdentity_ = 1;
    nextArrayIdentity_ = 1;
    Frame mainFrame;
    mainFrame.registers.assign(program.registerCount(), Value::nil());
    executeInstructions(program, program.instructions(), mainFrame, true);
}

IRInterpreter::ExecutionResult IRInterpreter::executeInstructions(
    const IRProgram& program,
    const std::vector<IRInstruction>& instructions,
    Frame& frame,
    bool isMain)
{

    std::size_t ip = 0;
    while (ip < instructions.size()) {
        const IRInstruction& instruction = instructions[ip];
        switch (instruction.op) {
        case IROp::Constant:
            writeRegister(frame, readDest(instruction), readConstant(program, instruction.operand));
            break;
        case IROp::MakeFunction: {
            if (instruction.operand >= program.functions().size()) {
                throw IRRuntimeError("function index out of range");
            }
            const IRFunction& function = program.functions()[instruction.operand];
            writeRegister(frame, readDest(instruction),
                Value::function(FunctionValue{
                    function.name,
                    instruction.operand,
                    function.parameters.size(),
                    nextFunctionIdentity_++,
                    captureEnvironment(frame),
                }));
            break;
        }
        case IROp::Array:
            writeRegister(frame, readDest(instruction), executeArray(instruction, frame));
            break;
        case IROp::Copy:
            writeRegister(frame, readDest(instruction), readRegister(frame, readLeft(instruction)));
            break;
        case IROp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            writeRegister(frame, readDest(instruction), loadVariable(frame, name));
            break;
        }
        case IROp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            storeVariable(frame, name, readRegister(frame, readLeft(instruction)), isMain);
            break;
        }
        case IROp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            assignVariable(frame, name, readRegister(frame, readLeft(instruction)));
            break;
        }
        case IROp::Call: {
            const Value& callee = readRegister(frame, readLeft(instruction));
            if (callee.type() != Value::Type::Function) {
                throw IRRuntimeError("can only call functions");
            }
            std::vector<Value> arguments;
            for (IRRegister argument : instruction.arguments) {
                arguments.push_back(readRegister(frame, argument));
            }
            writeRegister(frame, readDest(instruction), callFunction(program, callee.asFunction(), arguments));
            break;
        }
        case IROp::Index:
            writeRegister(frame, readDest(instruction), executeIndex(frame, readLeft(instruction), readRight(instruction)));
            break;
        case IROp::Len:
            writeRegister(frame, readDest(instruction), executeLen(frame, readLeft(instruction)));
            break;
        case IROp::Print:
            output_ << valueToString(readRegister(frame, readLeft(instruction))) << '\n';
            break;
        case IROp::Return:
            return ExecutionResult{true, readRegister(frame, readLeft(instruction))};
            break;
        case IROp::Negate:
            writeRegister(frame, readDest(instruction), executeUnaryNumber(frame, "negate", readLeft(instruction), [](double value) {
                return makeNumber(-value);
            }));
            break;
        case IROp::Not:
            writeRegister(frame, readDest(instruction), Value::boolean(!isTruthy(readRegister(frame, readLeft(instruction)))));
            break;
        case IROp::Add:
            writeRegister(frame, readDest(instruction), executeAdd(frame, readLeft(instruction), readRight(instruction)));
            break;
        case IROp::Subtract:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "subtract", readLeft(instruction), readRight(instruction), subtractNumber));
            break;
        case IROp::Multiply:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "multiply", readLeft(instruction), readRight(instruction), multiplyNumber));
            break;
        case IROp::Divide:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "divide", readLeft(instruction), readRight(instruction), divideNumber));
            break;
        case IROp::Equal:
            writeRegister(frame, readDest(instruction), Value::boolean(valuesEqual(readRegister(frame, readLeft(instruction)), readRegister(frame, readRight(instruction)))));
            break;
        case IROp::NotEqual:
            writeRegister(frame, readDest(instruction), Value::boolean(!valuesEqual(readRegister(frame, readLeft(instruction)), readRegister(frame, readRight(instruction)))));
            break;
        case IROp::Greater:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "greater", readLeft(instruction), readRight(instruction), greaterNumber));
            break;
        case IROp::GreaterEqual:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "greater_equal", readLeft(instruction), readRight(instruction), greaterEqualNumber));
            break;
        case IROp::Less:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "less", readLeft(instruction), readRight(instruction), lessNumber));
            break;
        case IROp::LessEqual:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "less_equal", readLeft(instruction), readRight(instruction), lessEqualNumber));
            break;
        case IROp::Jump:
            validateJumpTarget(instruction.operand, instructions.size());
            ip = instruction.operand;
            continue;
        case IROp::JumpIfFalse:
            validateJumpTarget(instruction.operand, instructions.size());
            if (!isTruthy(readRegister(frame, readLeft(instruction)))) {
                ip = instruction.operand;
                continue;
            }
            break;
        case IROp::JumpIfTrue:
            validateJumpTarget(instruction.operand, instructions.size());
            if (isTruthy(readRegister(frame, readLeft(instruction)))) {
                ip = instruction.operand;
                continue;
            }
            break;
        }

        ++ip;
    }
    return ExecutionResult{};
}

const std::unordered_map<std::string, Value>& IRInterpreter::globals() const
{
    refreshGlobalsView();
    return globalsView_;
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

const Value& IRInterpreter::readRegister(const Frame& frame, IRRegister reg) const
{
    if (reg.index >= frame.registers.size()) {
        throw IRRuntimeError("register index out of range");
    }
    return frame.registers[reg.index];
}

void IRInterpreter::writeRegister(Frame& frame, IRRegister reg, Value value)
{
    if (reg.index >= frame.registers.size()) {
        throw IRRuntimeError("register index out of range");
    }
    frame.registers[reg.index] = std::move(value);
}

std::shared_ptr<Cell> IRInterpreter::findCell(const Frame& frame, const std::string& name) const
{
    if (frame.locals) {
        const auto local = frame.locals->values.find(name);
        if (local != frame.locals->values.end()) {
            return local->second;
        }
    }

    if (frame.closure) {
        const auto captured = frame.closure->values.find(name);
        if (captured != frame.closure->values.end()) {
            return captured->second;
        }
    }

    if (globals_) {
        const auto global = globals_->values.find(name);
        if (global != globals_->values.end()) {
            return global->second;
        }
    }

    return nullptr;
}

Value IRInterpreter::loadVariable(const Frame& frame, const std::string& name) const
{
    std::shared_ptr<Cell> cell = findCell(frame, name);
    if (!cell) {
        throw IRRuntimeError("undefined variable `" + name + "`");
    }
    return cell->value;
}

void IRInterpreter::storeVariable(Frame& frame, const std::string& name, Value value, bool isMain)
{
    std::shared_ptr<Cell> cell = std::make_shared<Cell>(std::move(value));
    if (isMain) {
        globals_->values.insert_or_assign(name, std::move(cell));
        return;
    }
    frame.locals->values.insert_or_assign(name, std::move(cell));
}

void IRInterpreter::assignVariable(Frame& frame, const std::string& name, Value value)
{
    std::shared_ptr<Cell> cell = findCell(frame, name);
    if (!cell) {
        throw IRRuntimeError("undefined variable `" + name + "`");
    }
    cell->value = std::move(value);
}

std::shared_ptr<Environment> IRInterpreter::captureEnvironment(const Frame& frame) const
{
    auto captured = std::make_shared<Environment>();

    if (frame.closure) {
        for (const auto& [name, cell] : frame.closure->values) {
            captured->values.insert_or_assign(name, cell);
        }
    }

    if (frame.locals) {
        for (const auto& [name, cell] : frame.locals->values) {
            captured->values.insert_or_assign(name, cell);
        }
    }

    return captured;
}

void IRInterpreter::refreshGlobalsView() const
{
    globalsView_.clear();
    if (!globals_) {
        return;
    }
    for (const auto& [name, cell] : globals_->values) {
        globalsView_.emplace(name, cell->value);
    }
}

Value IRInterpreter::callFunction(const IRProgram& program, const FunctionValue& function, const std::vector<Value>& arguments)
{
    if (function.functionIndex >= program.functions().size()) {
        throw IRRuntimeError("function index out of range");
    }
    const IRFunction& irFunction = program.functions()[function.functionIndex];
    if (arguments.size() != irFunction.parameters.size()) {
        throw IRRuntimeError("expected " + std::to_string(irFunction.parameters.size())
            + " arguments but got " + std::to_string(arguments.size()));
    }

    Frame frame;
    frame.closure = function.closure ? function.closure : std::make_shared<Environment>();
    frame.registers.assign(irFunction.registerCount, Value::nil());
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        frame.locals->values.emplace(irFunction.parameters[i], std::make_shared<Cell>(arguments[i]));
    }

    ExecutionResult result = executeInstructions(program, irFunction.instructions, frame, false);
    return result.returned ? result.value : Value::nil();
}

Value IRInterpreter::executeArray(const IRInstruction& instruction, const Frame& frame)
{
    auto elements = std::make_shared<std::vector<Value>>();
    elements->reserve(instruction.arguments.size());
    for (IRRegister argument : instruction.arguments) {
        elements->push_back(readRegister(frame, argument));
    }
    return Value::array(ArrayValue{nextArrayIdentity_++, std::move(elements)});
}

Value IRInterpreter::executeIndex(const Frame& frame, IRRegister collection, IRRegister index)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw IRRuntimeError("can only index arrays");
    }

    const Value& indexValue = readRegister(frame, index);
    if (indexValue.type() != Value::Type::Number) {
        throw IRRuntimeError("array index must be number");
    }

    const double numericIndex = indexValue.asNumber();
    const double integerIndex = std::trunc(numericIndex);
    if (integerIndex != numericIndex) {
        throw IRRuntimeError("array index must be integer");
    }
    if (integerIndex < 0) {
        throw IRRuntimeError("array index out of range");
    }

    const auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw IRRuntimeError("array index out of range");
    }

    return elements[position];
}

Value IRInterpreter::executeLen(const Frame& frame, IRRegister value)
{
    const Value& input = readRegister(frame, value);
    if (input.type() == Value::Type::Array) {
        return Value::number(static_cast<double>(input.asArray().elements->size()));
    }
    if (input.type() == Value::Type::String) {
        return Value::number(static_cast<double>(input.asString().size()));
    }
    throw IRRuntimeError("len expects array or string");
}

Value IRInterpreter::executeUnaryNumber(const Frame& frame, const std::string& opName, IRRegister value, Value (*operation)(double))
{
    const Value& input = readRegister(frame, value);
    if (input.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects number, got " + typeName(input.type()));
    }
    return operation(input.asNumber());
}

Value IRInterpreter::executeBinaryNumber(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    const Value& leftValue = readRegister(frame, left);
    const Value& rightValue = readRegister(frame, right);
    if (leftValue.type() != Value::Type::Number || rightValue.type() != Value::Type::Number) {
        throw IRRuntimeError(opName + " expects numbers");
    }
    return operation(leftValue.asNumber(), rightValue.asNumber());
}

Value IRInterpreter::executeBinaryComparison(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double))
{
    return executeBinaryNumber(frame, opName, left, right, operation);
}

Value IRInterpreter::executeAdd(const Frame& frame, IRRegister left, IRRegister right)
{
    const Value& leftValue = readRegister(frame, left);
    const Value& rightValue = readRegister(frame, right);

    if (leftValue.type() == Value::Type::Number && rightValue.type() == Value::Type::Number) {
        return addNumber(leftValue.asNumber(), rightValue.asNumber());
    }

    if (leftValue.type() == Value::Type::String && rightValue.type() == Value::Type::String) {
        return Value::string(leftValue.asString() + rightValue.asString());
    }

    throw IRRuntimeError("add expects two numbers or two strings");
}
