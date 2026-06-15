#pragma once

#include "IR.hpp"

#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class IRRuntimeError final : public std::runtime_error {
public:
    explicit IRRuntimeError(const std::string& message);
};

class IRInterpreter {
public:
    explicit IRInterpreter(std::ostream& output);

    void execute(const IRProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    void push(Value value);
    Value pop();
    const Value& peek() const;

    Value readConstant(const IRProgram& program, std::size_t index) const;
    std::string readName(const IRProgram& program, std::size_t index) const;

    void executeUnaryNumber(const std::string& opName, Value (*operation)(double));
    void executeBinaryNumber(const std::string& opName, Value (*operation)(double, double));
    void executeBinaryComparison(const std::string& opName, Value (*operation)(double, double));
    void executeAdd();

    std::ostream& output_;
    std::vector<Value> stack_;
    std::unordered_map<std::string, Value> globals_;
};
