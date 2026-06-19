#pragma once

#include "Value.hpp"

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct IRRegister {
    std::size_t index = 0;
};

enum class IROp {
    Constant,
    LoadVar,
    StoreVar,
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
    std::optional<IRRegister> dest;
    std::optional<IRRegister> left;
    std::optional<IRRegister> right;
    std::size_t operand = 0;
};

class IRProgram {
public:
    std::size_t addConstant(Value value);
    std::size_t addName(std::string name);
    IRRegister makeRegister();

    IRRegister emitConstant(Value value);
    IRRegister emitLoadVar(std::string name);
    void emitStoreVar(std::string name, IRRegister value);
    void emitPrint(IRRegister value);
    IRRegister emitUnary(IROp op, IRRegister value);
    IRRegister emitBinary(IROp op, IRRegister left, IRRegister right);

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<IRInstruction>& instructions() const;
    std::size_t registerCount() const;

    // Print a compact, assembly-like view of the generated register IR.
    void print(std::ostream& out) const;

private:
    void emit(IRInstruction instruction);

    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<IRInstruction> instructions_;
    std::size_t registerCount_ = 0;
};

std::string irOpName(IROp op);
std::ostream& operator<<(std::ostream& out, IRRegister reg);
