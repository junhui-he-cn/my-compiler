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
    MakeFunction,
    Array,
    Struct,
    Copy,
    LoadVar,
    StoreVar,
    AssignVar,
    Call,
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

struct IRInstruction {
    IROp op;
    std::optional<IRRegister> dest;
    std::optional<IRRegister> left;
    std::optional<IRRegister> right;
    std::vector<IRRegister> arguments;
    std::size_t operand = 0;
    std::vector<std::size_t> operands;
};

struct IRFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<IRInstruction> instructions;
    std::size_t registerCount = 0;
};

class IRProgram {
public:
    std::size_t addConstant(Value value);
    std::size_t addName(std::string name);
    IRRegister makeRegister();
    void beginFunction(std::string name, std::vector<std::string> parameters);
    std::size_t endFunction();

    IRRegister emitConstant(Value value);
    IRRegister emitMakeFunction(std::size_t functionIndex);
    IRRegister emitArray(std::vector<IRRegister> elements);
    IRRegister emitStruct(std::vector<std::size_t> fieldNames, std::vector<IRRegister> fieldValues);
    IRRegister emitCopy(IRRegister value);
    void emitCopyTo(IRRegister dest, IRRegister value);
    IRRegister emitLoadVar(std::string name);
    void emitStoreVar(std::string name, IRRegister value);
    void emitAssignVar(std::string name, IRRegister value);
    IRRegister emitCall(IRRegister callee, std::vector<IRRegister> arguments);
    IRRegister emitIndex(IRRegister collection, IRRegister index);
    IRRegister emitAssignIndex(IRRegister collection, IRRegister index, IRRegister value);
    IRRegister emitField(IRRegister object, std::string fieldName);
    IRRegister emitAssignField(IRRegister object, std::string fieldName, IRRegister value);
    IRRegister emitLen(IRRegister value);
    void emitPrint(IRRegister value);
    void emitReturn(IRRegister value);
    IRRegister emitUnary(IROp op, IRRegister value);
    IRRegister emitBinary(IROp op, IRRegister left, IRRegister right);
    std::size_t emitJump();
    void emitJumpTo(std::size_t target);
    std::size_t emitJumpIfFalse(IRRegister condition);
    std::size_t emitJumpIfTrue(IRRegister condition);
    void patchJump(std::size_t jumpInstruction);
    std::size_t instructionCount() const;

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<IRInstruction>& instructions() const;
    const std::vector<IRFunction>& functions() const;
    std::size_t registerCount() const;

    // Print a compact, assembly-like view of the generated register IR.
    void print(std::ostream& out) const;

private:
    void emit(IRInstruction instruction);

    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<IRInstruction> instructions_;
    std::size_t registerCount_ = 0;
    std::vector<IRFunction> functionStack_;
    std::vector<IRFunction> functions_;
};

std::string irOpName(IROp op);
std::ostream& operator<<(std::ostream& out, IRRegister reg);
