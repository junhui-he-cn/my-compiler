#pragma once

#include "Bytecode.hpp"
#include "Diagnostic.hpp"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

class BytecodeRuntimeError final : public DiagnosticError {
public:
    explicit BytecodeRuntimeError(std::string message);
};

class VMHeap {
public:
    Value makeFunction(std::string name, std::size_t functionIndex, std::size_t arity, std::size_t identity, std::shared_ptr<Environment> closure);
    Value makeArray(std::size_t identity, std::shared_ptr<std::vector<Value>> elements);
};

class BytecodeVM {
public:
    explicit BytecodeVM(std::ostream& output);

    void execute(const BytecodeProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    struct VMFrame {
        std::uint32_t functionIndex = 0;
        std::size_t ip = 0;
        std::vector<Value> registers;
        std::shared_ptr<Environment> locals = std::make_shared<Environment>();
        std::shared_ptr<Environment> closure = std::make_shared<Environment>();
        bool isMain = false;
    };

    struct VMThread {
        std::vector<VMFrame> frames;
        bool halted = false;
        Value returnValue = Value::nil();
    };

    struct ExecutionResult {
        bool returned = false;
        Value value = Value::nil();
    };

    Value readConstant(const BytecodeProgram& program, std::size_t index) const;
    std::string readName(const BytecodeProgram& program, std::size_t index) const;
    BytecodeRegister readDest(const BytecodeInstruction& instruction) const;
    BytecodeRegister readLeft(const BytecodeInstruction& instruction) const;
    BytecodeRegister readRight(const BytecodeInstruction& instruction) const;
    ExecutionResult executeInstructions(const BytecodeProgram& program, const std::vector<BytecodeInstruction>& instructions, VMFrame& frame);
    Value callFunction(const BytecodeProgram& program, const FunctionValue& function, const std::vector<Value>& arguments);
    const Value& readRegister(const VMFrame& frame, BytecodeRegister reg) const;
    void writeRegister(VMFrame& frame, BytecodeRegister reg, Value value);
    void validateJumpTarget(std::size_t target, std::size_t instructionCount) const;

    std::shared_ptr<Cell> findCell(const VMFrame& frame, const std::string& name) const;
    Value loadVariable(const VMFrame& frame, const std::string& name) const;
    void storeVariable(VMFrame& frame, const std::string& name, Value value);
    void assignVariable(VMFrame& frame, const std::string& name, Value value);
    std::shared_ptr<Environment> captureEnvironment(const VMFrame& frame) const;
    void refreshGlobalsView() const;

    Value executeUnaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister left, BytecodeRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const VMFrame& frame, const std::string& opName, BytecodeRegister left, BytecodeRegister right, Value (*operation)(double, double));
    Value executeAdd(const VMFrame& frame, BytecodeRegister left, BytecodeRegister right);
    Value executeArray(const BytecodeInstruction& instruction, const VMFrame& frame);
    Value executeIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index);
    Value executeLen(const VMFrame& frame, BytecodeRegister value);

    std::ostream& output_;
    std::shared_ptr<Environment> globals_ = std::make_shared<Environment>();
    mutable std::unordered_map<std::string, Value> globalsView_;
    VMHeap heap_;
    std::size_t nextFunctionIdentity_ = 1;
    std::size_t nextArrayIdentity_ = 1;
};
