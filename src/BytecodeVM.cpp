#include "BytecodeVM.hpp"

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
        throw BytecodeRuntimeError("division by zero");
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

} // namespace

BytecodeRuntimeError::BytecodeRuntimeError(std::string message)
    : DiagnosticError(DiagnosticKind::Runtime, std::move(message))
{
}

Value VMHeap::makeFunction(std::string name, std::size_t functionIndex, std::size_t arity, std::size_t identity, std::shared_ptr<Environment> closure)
{
    return Value::function(FunctionValue{std::move(name), functionIndex, arity, identity, std::move(closure)});
}

Value VMHeap::makeArray(std::size_t identity, std::shared_ptr<std::vector<Value>> elements)
{
    return Value::array(ArrayValue{identity, std::move(elements)});
}

BytecodeVM::BytecodeVM(std::ostream& output)
    : output_(output)
{
}

void BytecodeVM::execute(const BytecodeProgram& program)
{
    globals_ = std::make_shared<Environment>();
    globalsView_.clear();
    nextFunctionIdentity_ = 1;
    nextArrayIdentity_ = 1;

    VMThread thread;
    VMFrame mainFrame;
    mainFrame.registers.assign(program.registerCount(), Value::nil());
    mainFrame.isMain = true;
    thread.frames.push_back(std::move(mainFrame));

    ExecutionResult result = executeInstructions(program, program.instructions(), thread.frames.back());
    if (result.returned) {
        thread.returnValue = result.value;
    }
    thread.halted = true;
}

BytecodeVM::ExecutionResult BytecodeVM::executeInstructions(
    const BytecodeProgram& program,
    const std::vector<BytecodeInstruction>& instructions,
    VMFrame& frame)
{
    frame.ip = 0;
    while (frame.ip < instructions.size()) {
        const BytecodeInstruction& instruction = instructions[frame.ip];
        switch (instruction.op) {
        case BytecodeOp::Constant:
            writeRegister(frame, readDest(instruction), readConstant(program, instruction.operand));
            break;
        case BytecodeOp::MakeFunction: {
            if (instruction.operand >= program.functions().size()) {
                throw BytecodeRuntimeError("function index out of range");
            }
            const BytecodeFunction& function = program.functions()[instruction.operand];
            writeRegister(frame, readDest(instruction),
                heap_.makeFunction(function.name,
                    instruction.operand,
                    function.parameters.size(),
                    nextFunctionIdentity_++,
                    captureEnvironment(frame)));
            break;
        }
        case BytecodeOp::Array:
            writeRegister(frame, readDest(instruction), executeArray(instruction, frame));
            break;
        case BytecodeOp::Move:
            writeRegister(frame, readDest(instruction), readRegister(frame, readLeft(instruction)));
            break;
        case BytecodeOp::LoadVar: {
            const std::string name = readName(program, instruction.operand);
            writeRegister(frame, readDest(instruction), loadVariable(frame, name));
            break;
        }
        case BytecodeOp::StoreVar: {
            const std::string name = readName(program, instruction.operand);
            storeVariable(frame, name, readRegister(frame, readLeft(instruction)));
            break;
        }
        case BytecodeOp::AssignVar: {
            const std::string name = readName(program, instruction.operand);
            assignVariable(frame, name, readRegister(frame, readLeft(instruction)));
            break;
        }
        case BytecodeOp::Call: {
            const Value& callee = readRegister(frame, readLeft(instruction));
            if (callee.type() != Value::Type::Function) {
                throw BytecodeRuntimeError("can only call functions");
            }
            std::vector<Value> arguments;
            arguments.reserve(instruction.arguments.size());
            for (BytecodeRegister argument : instruction.arguments) {
                arguments.push_back(readRegister(frame, argument));
            }
            writeRegister(frame, readDest(instruction), callFunction(program, callee.asFunction(), arguments));
            break;
        }
        case BytecodeOp::Index:
            writeRegister(frame, readDest(instruction), executeIndex(frame, readLeft(instruction), readRight(instruction)));
            break;
        case BytecodeOp::AssignIndex:
            writeRegister(frame,
                readDest(instruction),
                executeAssignIndex(frame, readLeft(instruction), readRight(instruction), instruction.arguments.at(0)));
            break;
        case BytecodeOp::Len:
            writeRegister(frame, readDest(instruction), executeLen(frame, readLeft(instruction)));
            break;
        case BytecodeOp::Print:
            output_ << valueToString(readRegister(frame, readLeft(instruction))) << '\n';
            break;
        case BytecodeOp::Return:
            return ExecutionResult{true, readRegister(frame, readLeft(instruction))};
        case BytecodeOp::Negate:
            writeRegister(frame, readDest(instruction), executeUnaryNumber(frame, "negate", readLeft(instruction), [](double value) {
                return makeNumber(-value);
            }));
            break;
        case BytecodeOp::Not:
            writeRegister(frame, readDest(instruction), Value::boolean(!isTruthy(readRegister(frame, readLeft(instruction)))));
            break;
        case BytecodeOp::Add:
            writeRegister(frame, readDest(instruction), executeAdd(frame, readLeft(instruction), readRight(instruction)));
            break;
        case BytecodeOp::Subtract:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "subtract", readLeft(instruction), readRight(instruction), subtractNumber));
            break;
        case BytecodeOp::Multiply:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "multiply", readLeft(instruction), readRight(instruction), multiplyNumber));
            break;
        case BytecodeOp::Divide:
            writeRegister(frame, readDest(instruction), executeBinaryNumber(frame, "divide", readLeft(instruction), readRight(instruction), divideNumber));
            break;
        case BytecodeOp::Equal:
            writeRegister(frame, readDest(instruction), Value::boolean(valuesEqual(readRegister(frame, readLeft(instruction)), readRegister(frame, readRight(instruction)))));
            break;
        case BytecodeOp::NotEqual:
            writeRegister(frame, readDest(instruction), Value::boolean(!valuesEqual(readRegister(frame, readLeft(instruction)), readRegister(frame, readRight(instruction)))));
            break;
        case BytecodeOp::Greater:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "greater", readLeft(instruction), readRight(instruction), greaterNumber));
            break;
        case BytecodeOp::GreaterEqual:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "greater_equal", readLeft(instruction), readRight(instruction), greaterEqualNumber));
            break;
        case BytecodeOp::Less:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "less", readLeft(instruction), readRight(instruction), lessNumber));
            break;
        case BytecodeOp::LessEqual:
            writeRegister(frame, readDest(instruction), executeBinaryComparison(frame, "less_equal", readLeft(instruction), readRight(instruction), lessEqualNumber));
            break;
        case BytecodeOp::Jump:
            validateJumpTarget(instruction.operand, instructions.size());
            frame.ip = instruction.operand;
            continue;
        case BytecodeOp::JumpIfFalse:
            validateJumpTarget(instruction.operand, instructions.size());
            if (!isTruthy(readRegister(frame, readLeft(instruction)))) {
                frame.ip = instruction.operand;
                continue;
            }
            break;
        case BytecodeOp::JumpIfTrue:
            validateJumpTarget(instruction.operand, instructions.size());
            if (isTruthy(readRegister(frame, readLeft(instruction)))) {
                frame.ip = instruction.operand;
                continue;
            }
            break;
        }

        ++frame.ip;
    }

    return ExecutionResult{};
}

const std::unordered_map<std::string, Value>& BytecodeVM::globals() const
{
    refreshGlobalsView();
    return globalsView_;
}

Value BytecodeVM::readConstant(const BytecodeProgram& program, std::size_t index) const
{
    if (index >= program.constants().size()) {
        throw BytecodeRuntimeError("constant index out of range");
    }
    return program.constants()[index];
}

std::string BytecodeVM::readName(const BytecodeProgram& program, std::size_t index) const
{
    if (index >= program.names().size()) {
        throw BytecodeRuntimeError("name index out of range");
    }
    return program.names()[index];
}

BytecodeRegister BytecodeVM::readDest(const BytecodeInstruction& instruction) const
{
    if (!instruction.dest) {
        throw BytecodeRuntimeError(bytecodeOpName(instruction.op) + " missing destination register");
    }
    return *instruction.dest;
}

BytecodeRegister BytecodeVM::readLeft(const BytecodeInstruction& instruction) const
{
    if (!instruction.left) {
        throw BytecodeRuntimeError(bytecodeOpName(instruction.op) + " missing left register");
    }
    return *instruction.left;
}

BytecodeRegister BytecodeVM::readRight(const BytecodeInstruction& instruction) const
{
    if (!instruction.right) {
        throw BytecodeRuntimeError(bytecodeOpName(instruction.op) + " missing right register");
    }
    return *instruction.right;
}

const Value& BytecodeVM::readRegister(const VMFrame& frame, BytecodeRegister reg) const
{
    if (reg.index >= frame.registers.size()) {
        throw BytecodeRuntimeError("register index out of range");
    }
    return frame.registers[reg.index];
}

void BytecodeVM::writeRegister(VMFrame& frame, BytecodeRegister reg, Value value)
{
    if (reg.index >= frame.registers.size()) {
        throw BytecodeRuntimeError("register index out of range");
    }
    frame.registers[reg.index] = std::move(value);
}

void BytecodeVM::validateJumpTarget(std::size_t target, std::size_t instructionCount) const
{
    if (target > instructionCount) {
        throw BytecodeRuntimeError("jump target out of range");
    }
}

Value BytecodeVM::callFunction(const BytecodeProgram& program, const FunctionValue& function, const std::vector<Value>& arguments)
{
    if (function.functionIndex >= program.functions().size()) {
        throw BytecodeRuntimeError("function index out of range");
    }
    const BytecodeFunction& bytecodeFunction = program.functions()[function.functionIndex];
    if (arguments.size() != bytecodeFunction.parameters.size()) {
        throw BytecodeRuntimeError("expected " + std::to_string(bytecodeFunction.parameters.size())
            + " arguments but got " + std::to_string(arguments.size()));
    }

    VMFrame frame;
    frame.functionIndex = static_cast<std::uint32_t>(function.functionIndex);
    frame.isMain = false;
    frame.closure = function.closure ? function.closure : std::make_shared<Environment>();
    frame.registers.assign(bytecodeFunction.registerCount, Value::nil());
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        frame.locals->values.emplace(bytecodeFunction.parameters[i], std::make_shared<Cell>(arguments[i]));
    }

    ExecutionResult result = executeInstructions(program, bytecodeFunction.instructions, frame);
    return result.returned ? result.value : Value::nil();
}

std::shared_ptr<Cell> BytecodeVM::findCell(const VMFrame& frame, const std::string& name) const
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

Value BytecodeVM::loadVariable(const VMFrame& frame, const std::string& name) const
{
    std::shared_ptr<Cell> cell = findCell(frame, name);
    if (!cell) {
        throw BytecodeRuntimeError("undefined variable `" + name + "`");
    }
    return cell->value;
}

void BytecodeVM::storeVariable(VMFrame& frame, const std::string& name, Value value)
{
    std::shared_ptr<Cell> cell = std::make_shared<Cell>(std::move(value));
    if (frame.isMain) {
        globals_->values.insert_or_assign(name, std::move(cell));
        return;
    }
    frame.locals->values.insert_or_assign(name, std::move(cell));
}

void BytecodeVM::assignVariable(VMFrame& frame, const std::string& name, Value value)
{
    std::shared_ptr<Cell> cell = findCell(frame, name);
    if (!cell) {
        throw BytecodeRuntimeError("undefined variable `" + name + "`");
    }
    cell->value = std::move(value);
}

std::shared_ptr<Environment> BytecodeVM::captureEnvironment(const VMFrame& frame) const
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

void BytecodeVM::refreshGlobalsView() const
{
    globalsView_.clear();
    if (!globals_) {
        return;
    }
    for (const auto& [name, cell] : globals_->values) {
        globalsView_.emplace(name, cell->value);
    }
}

Value BytecodeVM::executeUnaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister value, Value (*operation)(double))
{
    const Value& input = readRegister(frame, value);
    if (input.type() != Value::Type::Number) {
        throw BytecodeRuntimeError(opName + " expects number, got " + typeName(input.type()));
    }
    return operation(input.asNumber());
}

Value BytecodeVM::executeBinaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister left, BytecodeRegister right, Value (*operation)(double, double))
{
    const Value& leftValue = readRegister(frame, left);
    const Value& rightValue = readRegister(frame, right);
    if (leftValue.type() != Value::Type::Number || rightValue.type() != Value::Type::Number) {
        throw BytecodeRuntimeError(opName + " expects numbers");
    }
    return operation(leftValue.asNumber(), rightValue.asNumber());
}

Value BytecodeVM::executeBinaryComparison(const VMFrame& frame, const std::string& opName, BytecodeRegister left, BytecodeRegister right, Value (*operation)(double, double))
{
    return executeBinaryNumber(frame, opName, left, right, operation);
}

Value BytecodeVM::executeAdd(const VMFrame& frame, BytecodeRegister left, BytecodeRegister right)
{
    const Value& leftValue = readRegister(frame, left);
    const Value& rightValue = readRegister(frame, right);

    if (leftValue.type() == Value::Type::Number && rightValue.type() == Value::Type::Number) {
        return addNumber(leftValue.asNumber(), rightValue.asNumber());
    }

    if (leftValue.type() == Value::Type::String && rightValue.type() == Value::Type::String) {
        return Value::string(leftValue.asString() + rightValue.asString());
    }

    throw BytecodeRuntimeError("add expects two numbers or two strings");
}

Value BytecodeVM::executeArray(const BytecodeInstruction& instruction, const VMFrame& frame)
{
    auto elements = std::make_shared<std::vector<Value>>();
    elements->reserve(instruction.arguments.size());
    for (BytecodeRegister argument : instruction.arguments) {
        elements->push_back(readRegister(frame, argument));
    }
    return heap_.makeArray(nextArrayIdentity_++, std::move(elements));
}

Value BytecodeVM::executeIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw BytecodeRuntimeError("can only index arrays");
    }

    const Value& indexValue = readRegister(frame, index);
    if (indexValue.type() != Value::Type::Number) {
        throw BytecodeRuntimeError("array index must be number");
    }

    const double numericIndex = indexValue.asNumber();
    const double integerIndex = std::trunc(numericIndex);
    if (integerIndex != numericIndex) {
        throw BytecodeRuntimeError("array index must be integer");
    }
    if (integerIndex < 0) {
        throw BytecodeRuntimeError("array index out of range");
    }

    const auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw BytecodeRuntimeError("array index out of range");
    }

    return elements[position];
}

Value BytecodeVM::executeAssignIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index, BytecodeRegister value)
{
    const Value& collectionValue = readRegister(frame, collection);
    if (collectionValue.type() != Value::Type::Array) {
        throw BytecodeRuntimeError("can only assign array elements");
    }

    const Value& indexValue = readRegister(frame, index);
    if (indexValue.type() != Value::Type::Number) {
        throw BytecodeRuntimeError("array index must be number");
    }

    const double numericIndex = indexValue.asNumber();
    const double integerIndex = std::trunc(numericIndex);
    if (integerIndex != numericIndex) {
        throw BytecodeRuntimeError("array index must be integer");
    }
    if (integerIndex < 0) {
        throw BytecodeRuntimeError("array index out of range");
    }

    auto& elements = *collectionValue.asArray().elements;
    const auto position = static_cast<std::size_t>(integerIndex);
    if (position >= elements.size()) {
        throw BytecodeRuntimeError("array index out of range");
    }

    Value assignedValue = readRegister(frame, value);
    elements[position] = assignedValue;
    return assignedValue;
}

Value BytecodeVM::executeLen(const VMFrame& frame, BytecodeRegister value)
{
    const Value& input = readRegister(frame, value);
    if (input.type() == Value::Type::Array) {
        return Value::number(static_cast<double>(input.asArray().elements->size()));
    }
    if (input.type() == Value::Type::String) {
        return Value::number(static_cast<double>(input.asString().size()));
    }
    throw BytecodeRuntimeError("len expects array or string");
}
