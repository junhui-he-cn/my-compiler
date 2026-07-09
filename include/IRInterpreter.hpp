#pragma once

#include "Diagnostic.hpp"
#include "IR.hpp"

#include <memory>
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
        std::shared_ptr<Environment> locals = std::make_shared<Environment>();
        std::shared_ptr<Environment> closure = std::make_shared<Environment>();
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
    std::shared_ptr<Cell> findCell(const Frame& frame, const std::string& name) const;
    Value loadVariable(const Frame& frame, const std::string& name) const;
    void storeVariable(Frame& frame, const std::string& name, Value value, bool isMain);
    void assignVariable(Frame& frame, const std::string& name, Value value);
    std::shared_ptr<Environment> captureEnvironment(const Frame& frame) const;
    void refreshGlobalsView() const;

    Value executeUnaryNumber(const Frame& frame, const std::string& opName, IRRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const Frame& frame, const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeAdd(const Frame& frame, IRRegister left, IRRegister right);
    Value executeArray(const IRInstruction& instruction, const Frame& frame);
    Value executeStruct(const IRProgram& program, const IRInstruction& instruction, const Frame& frame);
    Value executeIndex(const Frame& frame, IRRegister collection, IRRegister index);
    Value executeAssignIndex(const Frame& frame, IRRegister collection, IRRegister index, IRRegister value);
    Value executeField(const IRProgram& program, const Frame& frame, IRRegister object, std::size_t fieldNameIndex);
    Value executeAssignField(const IRProgram& program, const Frame& frame, IRRegister object, std::size_t fieldNameIndex, IRRegister value);
    Value executeLen(const Frame& frame, IRRegister value);
    Value executeNativeCall(const IRProgram& program, const Frame& frame, std::size_t nameIndex, const std::vector<IRRegister>& arguments);
    Value executeNativePush(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativePop(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeFloor(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeCeil(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeSqrt(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeStr(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeSubstr(const Frame& frame, const std::vector<IRRegister>& arguments);
    Value executeNativeCharAt(const Frame& frame, const std::vector<IRRegister>& arguments);

    std::ostream& output_;
    std::shared_ptr<Environment> globals_ = std::make_shared<Environment>();
    mutable std::unordered_map<std::string, Value> globalsView_;
    std::size_t nextFunctionIdentity_ = 1;
    std::size_t nextArrayIdentity_ = 1;
    std::size_t nextStructIdentity_ = 1;
};
