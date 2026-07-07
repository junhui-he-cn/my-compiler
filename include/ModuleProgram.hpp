#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "SourceManager.hpp"

#include <string>

class ModuleAssemblyError final : public DiagnosticError {
public:
    explicit ModuleAssemblyError(std::string message);
};

Program buildModuleProgram(const SourceLoadResult& loadResult);
