#include "IR.hpp"

#include <iomanip>
#include <stdexcept>
#include <utility>

namespace {

bool isUnary(IROp op)
{
    return op == IROp::Negate || op == IROp::Not;
}

bool isBinary(IROp op)
{
    switch (op) {
    case IROp::Add:
    case IROp::Subtract:
    case IROp::Multiply:
    case IROp::Divide:
    case IROp::Equal:
    case IROp::NotEqual:
    case IROp::Greater:
    case IROp::GreaterEqual:
    case IROp::Less:
    case IROp::LessEqual:
        return true;
    case IROp::Constant:
    case IROp::MakeFunction:
    case IROp::Copy:
    case IROp::LoadVar:
    case IROp::StoreVar:
    case IROp::AssignVar:
    case IROp::Call:
    case IROp::Print:
    case IROp::Return:
    case IROp::Negate:
    case IROp::Not:
    case IROp::Jump:
    case IROp::JumpIfFalse:
    case IROp::JumpIfTrue:
        return false;
    }

    return false;
}

void printEscapedStringLiteral(std::ostream& out, const std::string& value)
{
    out << '"';
    for (char ch : value) {
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << ch;
            break;
        }
    }
    out << '"';
}

void printIRConstantValue(std::ostream& out, const Value& value)
{
    if (value.type() == Value::Type::String) {
        printEscapedStringLiteral(out, value.asString());
        return;
    }

    out << value;
}

void printConstantOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " #" << operand;
    if (operand < program.constants().size()) {
        out << " ";
        printIRConstantValue(out, program.constants()[operand]);
    }
}

void printNameOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " @" << operand;
    if (operand < program.names().size()) {
        out << " " << program.names()[operand];
    }
}

void printInstruction(std::ostream& out, const IRProgram& program, const IRInstruction& instruction, std::size_t index)
{
    out << std::setw(4) << std::setfill('0') << index << std::setfill(' ') << "  ";

    if (instruction.dest) {
        out << *instruction.dest << " = ";
    }

    out << irOpName(instruction.op);

    if (instruction.op == IROp::Constant) {
        printConstantOperand(out, program, instruction.operand);
    } else if (instruction.op == IROp::MakeFunction) {
        out << " $" << instruction.operand;
        if (instruction.operand < program.functions().size()) {
            const IRFunction& function = program.functions()[instruction.operand];
            out << " " << function.name << "/" << function.parameters.size();
        }
    } else if (instruction.op == IROp::Copy) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (instruction.op == IROp::LoadVar) {
        printNameOperand(out, program, instruction.operand);
    } else if (instruction.op == IROp::StoreVar || instruction.op == IROp::AssignVar) {
        printNameOperand(out, program, instruction.operand);
        if (instruction.left) {
            out << ", " << *instruction.left;
        }
    } else if (instruction.op == IROp::Call) {
        if (instruction.left) {
            out << " " << *instruction.left << "(";
            for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
                if (arg != 0) {
                    out << ", ";
                }
                out << instruction.arguments[arg];
            }
            out << ")";
        }
    } else if (instruction.op == IROp::Print) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (instruction.op == IROp::Return) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (isUnary(instruction.op)) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (isBinary(instruction.op)) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
    } else if (instruction.op == IROp::Jump) {
        out << " " << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
    } else if (instruction.op == IROp::JumpIfFalse || instruction.op == IROp::JumpIfTrue) {
        if (instruction.left) {
            out << " " << *instruction.left << ", ";
        } else {
            out << " ";
        }
        out << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
    }

    out << '\n';
}

} // namespace

std::size_t IRProgram::addConstant(Value value)
{
    constants_.push_back(std::move(value));
    return constants_.size() - 1;
}

std::size_t IRProgram::addName(std::string name)
{
    names_.push_back(std::move(name));
    return names_.size() - 1;
}

IRRegister IRProgram::makeRegister()
{
    if (buildingFunction_) {
        return IRRegister{currentFunction_.registerCount++};
    }
    return IRRegister{registerCount_++};
}

void IRProgram::beginFunction(std::string name, std::vector<std::string> parameters)
{
    if (buildingFunction_) {
        throw std::logic_error("nested IR function build");
    }
    buildingFunction_ = true;
    currentFunction_ = IRFunction{std::move(name), std::move(parameters), {}, 0};
}

std::size_t IRProgram::endFunction()
{
    if (!buildingFunction_) {
        throw std::logic_error("not building IR function");
    }
    buildingFunction_ = false;
    functions_.push_back(std::move(currentFunction_));
    currentFunction_ = IRFunction{};
    return functions_.size() - 1;
}

IRRegister IRProgram::emitConstant(Value value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Constant, dest, std::nullopt, std::nullopt, {}, addConstant(std::move(value))});
    return dest;
}

IRRegister IRProgram::emitMakeFunction(std::size_t functionIndex)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::MakeFunction, dest, std::nullopt, std::nullopt, {}, functionIndex});
    return dest;
}

IRRegister IRProgram::emitCopy(IRRegister value)
{
    IRRegister dest = makeRegister();
    emitCopyTo(dest, value);
    return dest;
}

void IRProgram::emitCopyTo(IRRegister dest, IRRegister value)
{
    emit(IRInstruction{IROp::Copy, dest, value, std::nullopt, {}, 0});
}

IRRegister IRProgram::emitLoadVar(std::string name)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::LoadVar, dest, std::nullopt, std::nullopt, {}, addName(std::move(name))});
    return dest;
}

void IRProgram::emitStoreVar(std::string name, IRRegister value)
{
    emit(IRInstruction{IROp::StoreVar, std::nullopt, value, std::nullopt, {}, addName(std::move(name))});
}

void IRProgram::emitAssignVar(std::string name, IRRegister value)
{
    emit(IRInstruction{IROp::AssignVar, std::nullopt, value, std::nullopt, {}, addName(std::move(name))});
}

IRRegister IRProgram::emitCall(IRRegister callee, std::vector<IRRegister> arguments)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Call, dest, callee, std::nullopt, std::move(arguments), 0});
    return dest;
}

void IRProgram::emitPrint(IRRegister value)
{
    emit(IRInstruction{IROp::Print, std::nullopt, value, std::nullopt, {}, 0});
}

void IRProgram::emitReturn(IRRegister value)
{
    emit(IRInstruction{IROp::Return, std::nullopt, value, std::nullopt, {}, 0});
}

IRRegister IRProgram::emitUnary(IROp op, IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, value, std::nullopt, {}, 0});
    return dest;
}

IRRegister IRProgram::emitBinary(IROp op, IRRegister left, IRRegister right)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, left, right, {}, 0});
    return dest;
}

std::size_t IRProgram::emitJump()
{
    const std::size_t instruction = instructionCount();
    emit(IRInstruction{IROp::Jump, std::nullopt, std::nullopt, std::nullopt, {}, 0});
    return instruction;
}

void IRProgram::emitJumpTo(std::size_t target)
{
    emit(IRInstruction{IROp::Jump, std::nullopt, std::nullopt, std::nullopt, {}, target});
}

std::size_t IRProgram::emitJumpIfFalse(IRRegister condition)
{
    const std::size_t instruction = instructionCount();
    emit(IRInstruction{IROp::JumpIfFalse, std::nullopt, condition, std::nullopt, {}, 0});
    return instruction;
}

std::size_t IRProgram::emitJumpIfTrue(IRRegister condition)
{
    const std::size_t instruction = instructionCount();
    emit(IRInstruction{IROp::JumpIfTrue, std::nullopt, condition, std::nullopt, {}, 0});
    return instruction;
}

void IRProgram::patchJump(std::size_t jumpInstruction)
{
    auto& instructions = buildingFunction_ ? currentFunction_.instructions : instructions_;
    if (jumpInstruction >= instructions.size()) {
        throw std::logic_error("jump instruction index out of range");
    }

    auto& instruction = instructions[jumpInstruction];
    if (instruction.op != IROp::Jump && instruction.op != IROp::JumpIfFalse
        && instruction.op != IROp::JumpIfTrue) {
        throw std::logic_error("cannot patch non-jump instruction");
    }

    instruction.operand = instructions.size();
}

const std::vector<Value>& IRProgram::constants() const
{
    return constants_;
}

const std::vector<std::string>& IRProgram::names() const
{
    return names_;
}

const std::vector<IRInstruction>& IRProgram::instructions() const
{
    return instructions_;
}

const std::vector<IRFunction>& IRProgram::functions() const
{
    return functions_;
}

std::size_t IRProgram::registerCount() const
{
    return registerCount_;
}

std::size_t IRProgram::instructionCount() const
{
    if (buildingFunction_) {
        return currentFunction_.instructions.size();
    }
    return instructions_.size();
}

void IRProgram::print(std::ostream& out) const
{
    out << "IR\n";
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        printInstruction(out, *this, instructions_[i], i);
    }

    for (std::size_t functionIndex = 0; functionIndex < functions_.size(); ++functionIndex) {
        const IRFunction& function = functions_[functionIndex];
        out << '\n' << "function $" << functionIndex << " " << function.name << "(";
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << function.parameters[i];
        }
        out << ")\n";
        for (std::size_t i = 0; i < function.instructions.size(); ++i) {
            printInstruction(out, *this, function.instructions[i], i);
        }
    }
}

void IRProgram::emit(IRInstruction instruction)
{
    if (buildingFunction_) {
        currentFunction_.instructions.push_back(std::move(instruction));
        return;
    }
    instructions_.push_back(std::move(instruction));
}

std::string irOpName(IROp op)
{
    switch (op) {
    case IROp::Constant:
        return "constant";
    case IROp::MakeFunction:
        return "make_function";
    case IROp::Copy:
        return "copy";
    case IROp::LoadVar:
        return "load_var";
    case IROp::StoreVar:
        return "store_var";
    case IROp::AssignVar:
        return "assign_var";
    case IROp::Call:
        return "call";
    case IROp::Print:
        return "print";
    case IROp::Return:
        return "return";
    case IROp::Negate:
        return "negate";
    case IROp::Not:
        return "not";
    case IROp::Add:
        return "add";
    case IROp::Subtract:
        return "subtract";
    case IROp::Multiply:
        return "multiply";
    case IROp::Divide:
        return "divide";
    case IROp::Equal:
        return "equal";
    case IROp::NotEqual:
        return "not_equal";
    case IROp::Greater:
        return "greater";
    case IROp::GreaterEqual:
        return "greater_equal";
    case IROp::Less:
        return "less";
    case IROp::LessEqual:
        return "less_equal";
    case IROp::Jump:
        return "jump";
    case IROp::JumpIfFalse:
        return "jump_if_false";
    case IROp::JumpIfTrue:
        return "jump_if_true";
    }

    return "unknown";
}

std::ostream& operator<<(std::ostream& out, IRRegister reg)
{
    out << 'v' << reg.index;
    return out;
}
