#include "BytecodeCompiler.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace {

std::uint32_t checkedU32(std::size_t value, const char* message)
{
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw BytecodeCompileError(message);
    }
    return static_cast<std::uint32_t>(value);
}

BytecodeRegister lowerRegister(IRRegister reg)
{
    return BytecodeRegister{checkedU32(reg.index, "register index out of range")};
}

std::optional<BytecodeRegister> lowerRegister(std::optional<IRRegister> reg)
{
    if (!reg) {
        return std::nullopt;
    }
    return lowerRegister(*reg);
}

std::vector<BytecodeRegister> lowerRegisters(const std::vector<IRRegister>& registers)
{
    std::vector<BytecodeRegister> lowered;
    lowered.reserve(registers.size());
    for (IRRegister reg : registers) {
        lowered.push_back(lowerRegister(reg));
    }
    return lowered;
}

std::vector<std::uint32_t> lowerOperands(const std::vector<std::size_t>& operands)
{
    std::vector<std::uint32_t> lowered;
    lowered.reserve(operands.size());
    for (std::size_t operand : operands) {
        lowered.push_back(checkedU32(operand, "operand out of range"));
    }
    return lowered;
}

BytecodeOp lowerOp(IROp op)
{
    switch (op) {
    case IROp::Constant:
        return BytecodeOp::Constant;
    case IROp::MakeFunction:
        return BytecodeOp::MakeFunction;
    case IROp::Array:
        return BytecodeOp::Array;
    case IROp::Struct:
        return BytecodeOp::Struct;
    case IROp::Copy:
        return BytecodeOp::Move;
    case IROp::LoadVar:
        return BytecodeOp::LoadVar;
    case IROp::StoreVar:
        return BytecodeOp::StoreVar;
    case IROp::AssignVar:
        return BytecodeOp::AssignVar;
    case IROp::Call:
        return BytecodeOp::Call;
    case IROp::NativeCall:
        return BytecodeOp::NativeCall;
    case IROp::Index:
        return BytecodeOp::Index;
    case IROp::AssignIndex:
        return BytecodeOp::AssignIndex;
    case IROp::Field:
        return BytecodeOp::Field;
    case IROp::AssignField:
        return BytecodeOp::AssignField;
    case IROp::Len:
        return BytecodeOp::Len;
    case IROp::AssertArray:
        return BytecodeOp::AssertArray;
    case IROp::Print:
        return BytecodeOp::Print;
    case IROp::Return:
        return BytecodeOp::Return;
    case IROp::Negate:
        return BytecodeOp::Negate;
    case IROp::Not:
        return BytecodeOp::Not;
    case IROp::Add:
        return BytecodeOp::Add;
    case IROp::Subtract:
        return BytecodeOp::Subtract;
    case IROp::Multiply:
        return BytecodeOp::Multiply;
    case IROp::Divide:
        return BytecodeOp::Divide;
    case IROp::Equal:
        return BytecodeOp::Equal;
    case IROp::NotEqual:
        return BytecodeOp::NotEqual;
    case IROp::Greater:
        return BytecodeOp::Greater;
    case IROp::GreaterEqual:
        return BytecodeOp::GreaterEqual;
    case IROp::Less:
        return BytecodeOp::Less;
    case IROp::LessEqual:
        return BytecodeOp::LessEqual;
    case IROp::Jump:
        return BytecodeOp::Jump;
    case IROp::JumpIfFalse:
        return BytecodeOp::JumpIfFalse;
    case IROp::JumpIfTrue:
        return BytecodeOp::JumpIfTrue;
    }

    return BytecodeOp::Constant;
}

} // namespace

BytecodeCompileError::BytecodeCompileError(std::string message)
    : DiagnosticError(DiagnosticKind::Compile, std::move(message))
{
}

BytecodeProgram BytecodeCompiler::compile(const IRProgram& ir)
{
    BytecodeProgram program;
    program.setConstants(ir.constants());
    program.setNames(ir.names());
    program.setRegisterCount(checkedU32(ir.registerCount(), "register index out of range"));
    program.setInstructions(lowerInstructions(ir.instructions()));

    std::vector<BytecodeFunction> functions;
    functions.reserve(ir.functions().size());
    for (const IRFunction& function : ir.functions()) {
        functions.push_back(lowerFunction(function));
    }
    program.setFunctions(std::move(functions));

    return program;
}

std::vector<BytecodeInstruction> BytecodeCompiler::lowerInstructions(const std::vector<IRInstruction>& instructions)
{
    std::vector<BytecodeInstruction> lowered;
    lowered.reserve(instructions.size());
    for (const IRInstruction& instruction : instructions) {
        lowered.push_back(lowerInstruction(instruction));
    }
    return lowered;
}

BytecodeInstruction BytecodeCompiler::lowerInstruction(const IRInstruction& instruction)
{
    return BytecodeInstruction{
        lowerOp(instruction.op),
        lowerRegister(instruction.dest),
        lowerRegister(instruction.left),
        lowerRegister(instruction.right),
        lowerRegisters(instruction.arguments),
        checkedU32(instruction.operand, "operand out of range"),
        lowerOperands(instruction.operands)};
}

BytecodeFunction BytecodeCompiler::lowerFunction(const IRFunction& function)
{
    return BytecodeFunction{
        function.name,
        function.parameters,
        lowerInstructions(function.instructions),
        checkedU32(function.registerCount, "register index out of range")};
}
