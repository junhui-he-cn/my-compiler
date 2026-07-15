#pragma once

#include "Token.hpp"
#include "TypeUtils.hpp"

#include <cstddef>
#include <optional>
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

struct EnumVariantType {
    Token name;
    std::vector<TypeInfo> payloadTypes;
    std::vector<std::optional<Token>> payloadNames;
};

struct EnumTypeDecl {
    Token name;
    std::vector<std::string> genericParameters;
    std::vector<EnumVariantType> variants;
};

struct MethodSignature {
    TypeInfo receiverType;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
    std::string resolvedName;
    std::vector<std::string> genericParameters;
};
