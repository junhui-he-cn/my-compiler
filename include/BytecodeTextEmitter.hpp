#pragma once

#include "Bytecode.hpp"

#include <ostream>

void writeBytecodeText(std::ostream& out, const BytecodeProgram& program);
