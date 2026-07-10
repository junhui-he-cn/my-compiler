#pragma once

#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct TypeBinding {
    TypeInfo type;
    std::string resolvedName;
    std::size_t scopeDepth = 0;
    std::size_t functionDepth = 0;
    bool explicitType = false;
    bool imported = false;
};

struct StructFieldType {
    Token name;
    TypeInfo type;
};

struct StructTypeDecl {
    Token name;
    std::vector<StructFieldType> fields;
};
