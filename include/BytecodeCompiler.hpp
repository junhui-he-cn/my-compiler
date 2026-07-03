#pragma once

#include "Bytecode.hpp"
#include "Diagnostic.hpp"
#include "IR.hpp"

#include <string>
#include <vector>

class BytecodeCompileError final : public DiagnosticError {
public:
    explicit BytecodeCompileError(std::string message);
};

class BytecodeCompiler {
public:
    BytecodeProgram compile(const IRProgram& ir);

private:
    std::vector<BytecodeInstruction> lowerInstructions(const std::vector<IRInstruction>& instructions);
    BytecodeInstruction lowerInstruction(const IRInstruction& instruction);
    BytecodeFunction lowerFunction(const IRFunction& function);
};
