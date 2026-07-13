#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class StaticType {
    Unknown,
    Nil,
    Number,
    Bool,
    String,
    Function,
    Array,
    Struct,
    Nullable,
    TypeParameter,
};

struct TypeInfo {
    StaticType kind = StaticType::Unknown;
    std::vector<TypeInfo> parameterTypes;
    std::shared_ptr<TypeInfo> returnType;
    std::optional<std::string> structName;
    std::shared_ptr<TypeInfo> elementType;
    std::shared_ptr<TypeInfo> nullableOf;
    std::optional<std::string> typeParameterName;
    std::vector<std::string> genericParameters;
};

TypeInfo unknownType();
TypeInfo simpleType(StaticType kind);
TypeInfo namedStructType(std::string name);
TypeInfo arrayType(TypeInfo elementType);
TypeInfo typeParameterType(std::string name);
TypeInfo functionType(
    std::vector<TypeInfo> parameterTypes,
    TypeInfo returnType,
    std::vector<std::string> genericParameters = {});
TypeInfo nullableType(TypeInfo innerType);
TypeInfo functionWithoutSignature();

bool isKnown(const TypeInfo& type);
bool hasFunctionSignature(const TypeInfo& type);
bool isNullable(const TypeInfo& type);
bool compatible(const TypeInfo& expected, const TypeInfo& actual);
std::optional<TypeInfo> mergeArrayElementTypes(const TypeInfo& left, const TypeInfo& right);

std::string staticTypeName(StaticType type);
std::string typeInfoName(const TypeInfo& type);
