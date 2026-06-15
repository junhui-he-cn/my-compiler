#include "IR.hpp"

#include <iomanip>
#include <utility>

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

void IRProgram::emit(IROp op, std::size_t operand)
{
    instructions_.push_back(IRInstruction{op, operand});
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

void IRProgram::print(std::ostream& out) const
{
    out << "IR\n";
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const IRInstruction& instruction = instructions_[i];
        out << std::setw(4) << std::setfill('0') << i << std::setfill(' ')
            << "  " << irOpName(instruction.op);

        if (instruction.op == IROp::Constant) {
            out << " #" << instruction.operand;
            if (instruction.operand < constants_.size()) {
                out << " " << constants_[instruction.operand];
            }
        } else if (instruction.op == IROp::LoadVar || instruction.op == IROp::StoreVar) {
            out << " @" << instruction.operand;
            if (instruction.operand < names_.size()) {
                out << " " << names_[instruction.operand];
            }
        }
        out << '\n';
    }
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
    case IROp::Pop:
        return "pop";
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
