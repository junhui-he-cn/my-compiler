#include "IR.hpp"

#include <iomanip>
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
    case IROp::LoadVar:
    case IROp::StoreVar:
    case IROp::Print:
    case IROp::Negate:
    case IROp::Not:
        return false;
    }

    return false;
}

void printConstantOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " #" << operand;
    if (operand < program.constants().size()) {
        out << " " << program.constants()[operand];
    }
}

void printNameOperand(std::ostream& out, const IRProgram& program, std::size_t operand)
{
    out << " @" << operand;
    if (operand < program.names().size()) {
        out << " " << program.names()[operand];
    }
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
    return IRRegister{registerCount_++};
}

IRRegister IRProgram::emitConstant(Value value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::Constant, dest, std::nullopt, std::nullopt, addConstant(std::move(value))});
    return dest;
}

IRRegister IRProgram::emitLoadVar(std::string name)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{IROp::LoadVar, dest, std::nullopt, std::nullopt, addName(std::move(name))});
    return dest;
}

void IRProgram::emitStoreVar(std::string name, IRRegister value)
{
    emit(IRInstruction{IROp::StoreVar, std::nullopt, value, std::nullopt, addName(std::move(name))});
}

void IRProgram::emitPrint(IRRegister value)
{
    emit(IRInstruction{IROp::Print, std::nullopt, value, std::nullopt, 0});
}

IRRegister IRProgram::emitUnary(IROp op, IRRegister value)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, value, std::nullopt, 0});
    return dest;
}

IRRegister IRProgram::emitBinary(IROp op, IRRegister left, IRRegister right)
{
    IRRegister dest = makeRegister();
    emit(IRInstruction{op, dest, left, right, 0});
    return dest;
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

std::size_t IRProgram::registerCount() const
{
    return registerCount_;
}

void IRProgram::print(std::ostream& out) const
{
    out << "IR\n";
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const IRInstruction& instruction = instructions_[i];
        out << std::setw(4) << std::setfill('0') << i << std::setfill(' ') << "  ";

        if (instruction.dest) {
            out << *instruction.dest << " = ";
        }

        out << irOpName(instruction.op);

        if (instruction.op == IROp::Constant) {
            printConstantOperand(out, *this, instruction.operand);
        } else if (instruction.op == IROp::LoadVar) {
            printNameOperand(out, *this, instruction.operand);
        } else if (instruction.op == IROp::StoreVar) {
            printNameOperand(out, *this, instruction.operand);
            if (instruction.left) {
                out << ", " << *instruction.left;
            }
        } else if (instruction.op == IROp::Print) {
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
        }

        out << '\n';
    }
}

void IRProgram::emit(IRInstruction instruction)
{
    instructions_.push_back(std::move(instruction));
}

std::string irOpName(IROp op)
{
    switch (op) {
    case IROp::Constant:
        return "constant";
    case IROp::LoadVar:
        return "load_var";
    case IROp::StoreVar:
        return "store_var";
    case IROp::Print:
        return "print";
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
    }

    return "unknown";
}

std::ostream& operator<<(std::ostream& out, IRRegister reg)
{
    out << 'v' << reg.index;
    return out;
}
