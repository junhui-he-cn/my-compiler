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
    Value readConstant(const IRProgram& program, std::size_t index) const;
    std::string readName(const IRProgram& program, std::size_t index) const;
    IRRegister readDest(const IRInstruction& instruction) const;
    IRRegister readLeft(const IRInstruction& instruction) const;
    IRRegister readRight(const IRInstruction& instruction) const;
    const Value& readRegister(IRRegister reg) const;
    void writeRegister(IRRegister reg, Value value);

    Value executeUnaryNumber(const std::string& opName, IRRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeBinaryComparison(const std::string& opName, IRRegister left, IRRegister right, Value (*operation)(double, double));
    Value executeAdd(IRRegister left, IRRegister right);

    std::ostream& output_;
    std::vector<Value> registers_;
    std::unordered_map<std::string, Value> globals_;
};
