#pragma once

#include "Value.hpp"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct BytecodeRegister {
    std::uint32_t index = 0;
};

enum class BytecodeOp {
    Constant,
    MakeFunction,
    Array,
    Struct,
    Move,
    LoadVar,
    StoreVar,
    AssignVar,
    Call,
    NativeCall,
    Index,
    AssignIndex,
    Field,
    AssignField,
    Len,
    Print,
    Return,
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
    Jump,
    JumpIfFalse,
    JumpIfTrue,
};

struct BytecodeInstruction {
    BytecodeOp op;
    std::optional<BytecodeRegister> dest;
    std::optional<BytecodeRegister> left;
    std::optional<BytecodeRegister> right;
    std::vector<BytecodeRegister> arguments;
    std::uint32_t operand = 0;
    std::vector<std::uint32_t> operands;
};

struct BytecodeFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<BytecodeInstruction> instructions;
    std::uint32_t registerCount = 0;
};

class BytecodeProgram {
public:
    void setConstants(std::vector<Value> constants);
    void setNames(std::vector<std::string> names);
    void setInstructions(std::vector<BytecodeInstruction> instructions);
    void setRegisterCount(std::uint32_t registerCount);
    void setFunctions(std::vector<BytecodeFunction> functions);

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<BytecodeInstruction>& instructions() const;
    std::uint32_t registerCount() const;
    const std::vector<BytecodeFunction>& functions() const;

    void print(std::ostream& out) const;

private:
    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<BytecodeInstruction> instructions_;
    std::uint32_t registerCount_ = 0;
    std::vector<BytecodeFunction> functions_;
};

std::string bytecodeOpName(BytecodeOp op);
std::ostream& operator<<(std::ostream& out, BytecodeRegister reg);
