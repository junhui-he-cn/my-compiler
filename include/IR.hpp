#pragma once

#include "Value.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
    Pop,
    Print,
    Negate,
    Not,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
};

struct IRInstruction {
    IROp op;
    std::size_t operand = 0;
};

class IRProgram {
public:
    std::size_t addConstant(Value value);
    std::size_t addName(std::string name);
    void emit(IROp op, std::size_t operand = 0);

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<IRInstruction>& instructions() const;

    // Print a compact, assembly-like view of the generated IR.
    void print(std::ostream& out) const;

private:
    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<IRInstruction> instructions_;
};

std::string irOpName(IROp op);
