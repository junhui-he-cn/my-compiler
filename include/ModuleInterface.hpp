#pragma once

#include "TypeUtils.hpp"

#include <cstddef>
#include <optional>
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
    std::vector<std::string> genericParameters;
};

struct ModuleInterfaceStruct {
    std::string name;
    std::vector<ModuleInterfaceField> fields;
    std::vector<ModuleInterfaceMethod> methods;
};

struct ModuleInterfaceVariant {
    std::string name;
    std::vector<TypeInfo> payloadTypes;
    std::vector<std::optional<std::string>> payloadNames;
};

struct ModuleInterfaceEnum {
    std::string name;
    std::vector<ModuleInterfaceVariant> variants;
};

struct ModuleInterface {
    std::size_t moduleId = 0;
    std::string path;
    bool isEntry = false;
    std::vector<ModuleInterfaceValue> values;
    std::vector<ModuleInterfaceStruct> structs;
    std::vector<ModuleInterfaceEnum> enums;
};
