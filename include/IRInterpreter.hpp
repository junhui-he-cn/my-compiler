#pragma once

#include "Diagnostic.hpp"
#include "IR.hpp"

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

class IRRuntimeError final : public DiagnosticError {
public:
    explicit IRRuntimeError(std::string message);
};

class IRInterpreter {
public:
    explicit IRInterpreter(std::ostream& output);

    void execute(const IRProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    struct Frame {
        std::vector<Value> registers;
        std::unordered_map<std::string, Value> locals;
    };

    struct ExecutionResult {
        bool returned = false;
        Value value = Value::nil();
    };

    Value readConstant(const IRProgram& program, std::size_t index) const;
    std::string readName(const IRProgram& program, std::size_t index) const;
    IRRegister readDest(const IRInstruction& instruction) const;
    IRRegister readLeft(const IRInstruction& instruction) const;
    IRRegister readRight(const IRInstruction& instruction) const;
    ExecutionResult executeInstructions(const IRProgram& program, const std::vector<IRInstruction>& instructions, Frame& frame, bool isMain);
    Value callFunction(const IRProgram& program, const FunctionValue& function, const std::vector<Value>& arguments);
    const Value& readRegister(const Frame& frame, IRRegister reg) const;
    void writeRegister(Frame& frame, IRRegister reg, Value value);
    Value loadVariable(const Frame& frame, const std::string& name) const;
    void assignVariable(Frame& frame, const std::string& name, Value value);

    Value executeUnaryNumber(const Frame& frame, const std::string& opName, IRRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeAdd(const Frame& frame, IRRegister left, IRRegister right);

    std::ostream& output_;
    std::unordered_map<std::string, Value> globals_;
};
