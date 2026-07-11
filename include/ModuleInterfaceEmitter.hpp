#pragma once

#include "ModuleInterface.hpp"

#include <iosfwd>
#include <vector>

void writeModuleInterfaceText(std::ostream& out, const std::vector<ModuleInterface>& interfaces);
