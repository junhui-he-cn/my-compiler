#pragma once

#include "TypeUtils.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct ModuleInterfaceValue {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceField {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceMethod {
    std::string name;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
};

struct ModuleInterfaceStruct {
    std::string name;
    std::vector<ModuleInterfaceField> fields;
    std::vector<ModuleInterfaceMethod> methods;
};

struct ModuleInterface {
    std::size_t moduleId = 0;
    std::string path;
    bool isEntry = false;
    std::vector<ModuleInterfaceValue> values;
    std::vector<ModuleInterfaceStruct> structs;
};
