#include "TypeUtils.hpp"

#include <cstddef>
#include <optional>
#include <utility>

TypeInfo unknownType()
{
    return TypeInfo{};
}

TypeInfo simpleType(StaticType kind)
{
    TypeInfo result;
    result.kind = kind;
    return result;
}

TypeInfo namedStructType(std::string name)
{
    TypeInfo result;
    result.kind = StaticType::Struct;
    result.structName = std::move(name);
    return result;
}

TypeInfo arrayType(TypeInfo elementType)
{
    TypeInfo result;
    result.kind = StaticType::Array;
    result.elementType = std::make_shared<TypeInfo>(std::move(elementType));
    return result;
}

TypeInfo mapType(TypeInfo keyType, TypeInfo valueType)
{
    TypeInfo result;
    result.kind = StaticType::Map;
    result.keyType = std::make_shared<TypeInfo>(std::move(keyType));
    result.valueType = std::make_shared<TypeInfo>(std::move(valueType));
    return result;
}

TypeInfo typeParameterType(std::string name)
{
    TypeInfo result;
    result.kind = StaticType::TypeParameter;
    result.typeParameterName = std::move(name);
    return result;
}

TypeInfo functionType(
    std::vector<TypeInfo> parameterTypes,
    TypeInfo returnType,
    std::vector<std::string> genericParameters)
{
    TypeInfo result;
    result.kind = StaticType::Function;
    result.parameterTypes = std::move(parameterTypes);
    result.returnType = std::make_shared<TypeInfo>(std::move(returnType));
    result.genericParameters = std::move(genericParameters);
    return result;
}

TypeInfo nullableType(TypeInfo innerType)
{
    TypeInfo result;
    result.kind = StaticType::Nullable;
    result.nullableOf = std::make_shared<TypeInfo>(std::move(innerType));
    return result;
}

TypeInfo functionWithoutSignature()
{
    TypeInfo result;
    result.kind = StaticType::Function;
    return result;
}

bool isKnown(const TypeInfo& type)
{
    return type.kind != StaticType::Unknown;
}

bool hasFunctionSignature(const TypeInfo& type)
{
    return type.kind == StaticType::Function && type.returnType != nullptr;
}

bool isNullable(const TypeInfo& type)
{
    return type.kind == StaticType::Nullable && type.nullableOf != nullptr;
}

std::string staticTypeName(StaticType type)
{
    switch (type) {
    case StaticType::Unknown:
        return "unknown";
    case StaticType::Nil:
        return "nil";
    case StaticType::Number:
        return "number";
    case StaticType::Bool:
        return "bool";
    case StaticType::String:
        return "string";
    case StaticType::Function:
        return "function";
    case StaticType::Array:
        return "array";
    case StaticType::Map:
        return "map";
    case StaticType::Range:
        return "range";
    case StaticType::Struct:
        return "struct";
    case StaticType::Nullable:
        return "nullable";
    case StaticType::TypeParameter:
        return "type parameter";
    }

    return "unknown";
}

std::string typeInfoName(const TypeInfo& type)
{
    if (isNullable(type)) {
        return typeInfoName(*type.nullableOf) + "?";
    }

    if (type.kind == StaticType::Struct && type.structName) {
        return *type.structName;
    }

    if (type.kind == StaticType::Array && type.elementType) {
        return "[" + typeInfoName(*type.elementType) + "]";
    }

    if (type.kind == StaticType::Map && type.keyType && type.valueType) {
        return "map<" + typeInfoName(*type.keyType) + ", " + typeInfoName(*type.valueType) + ">";
    }

    if (type.kind == StaticType::TypeParameter && type.typeParameterName) {
        return *type.typeParameterName;
    }

    if (type.kind != StaticType::Function || !type.returnType) {
        return staticTypeName(type.kind);
    }

    std::string result = "fun";
    if (!type.genericParameters.empty()) {
        result += '<';
        for (std::size_t i = 0; i < type.genericParameters.size(); ++i) {
            if (i != 0) {
                result += ", ";
            }
            result += type.genericParameters[i];
        }
        result += '>';
    }
    result += '(';
    for (std::size_t i = 0; i < type.parameterTypes.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += typeInfoName(type.parameterTypes[i]);
    }
    result += "): ";
    result += typeInfoName(*type.returnType);
    return result;
}

bool compatible(const TypeInfo& expected, const TypeInfo& actual)
{
    if (!isKnown(expected) || !isKnown(actual)) {
        return true;
    }
    if (isNullable(expected)) {
        if (actual.kind == StaticType::Nil) {
            return true;
        }
        if (isNullable(actual)) {
            return compatible(*expected.nullableOf, *actual.nullableOf);
        }
        return compatible(*expected.nullableOf, actual);
    }
    if (isNullable(actual)) {
        return false;
    }
    if (expected.kind == StaticType::TypeParameter || actual.kind == StaticType::TypeParameter) {
        return expected.kind == StaticType::TypeParameter
            && actual.kind == StaticType::TypeParameter
            && expected.typeParameterName == actual.typeParameterName;
    }
    if (expected.kind != actual.kind) {
        return false;
    }
    if (expected.kind == StaticType::Struct) {
        if (expected.structName || actual.structName) {
            return expected.structName == actual.structName;
        }
        return true;
    }
    if (expected.kind == StaticType::Array) {
        if (!expected.elementType || !actual.elementType) {
            return true;
        }
        return compatible(*expected.elementType, *actual.elementType);
    }
    if (expected.kind == StaticType::Map) {
        if (!expected.keyType || !actual.keyType || !expected.valueType || !actual.valueType) {
            return true;
        }
        return compatible(*expected.keyType, *actual.keyType)
            && compatible(*expected.valueType, *actual.valueType);
    }
    if (expected.kind != StaticType::Function) {
        return true;
    }
    if (!hasFunctionSignature(expected) || !hasFunctionSignature(actual)) {
        return true;
    }
    if (expected.parameterTypes.size() != actual.parameterTypes.size()) {
        return false;
    }
    for (std::size_t i = 0; i < expected.parameterTypes.size(); ++i) {
        if (!compatible(expected.parameterTypes[i], actual.parameterTypes[i])) {
            return false;
        }
    }
    return compatible(*expected.returnType, *actual.returnType);
}

std::optional<TypeInfo> mergeArrayElementTypes(const TypeInfo& left, const TypeInfo& right)
{
    if (!isKnown(left) || !isKnown(right)) {
        return std::nullopt;
    }

    if (left.kind == StaticType::Nil && right.kind == StaticType::Nil) {
        return simpleType(StaticType::Nil);
    }

    if (isNullable(left)) {
        if (right.kind == StaticType::Nil) {
            return left;
        }
        if (isNullable(right)) {
            std::optional<TypeInfo> inner = mergeArrayElementTypes(*left.nullableOf, *right.nullableOf);
            if (!inner) {
                return std::nullopt;
            }
            return nullableType(std::move(*inner));
        }
        std::optional<TypeInfo> inner = mergeArrayElementTypes(*left.nullableOf, right);
        if (!inner) {
            return std::nullopt;
        }
        return nullableType(std::move(*inner));
    }

    if (isNullable(right)) {
        if (left.kind == StaticType::Nil) {
            return right;
        }
        std::optional<TypeInfo> inner = mergeArrayElementTypes(left, *right.nullableOf);
        if (!inner) {
            return std::nullopt;
        }
        return nullableType(std::move(*inner));
    }

    if (left.kind == StaticType::Nil) {
        return nullableType(right);
    }
    if (right.kind == StaticType::Nil) {
        return nullableType(left);
    }

    if (left.kind == StaticType::Array && right.kind == StaticType::Array) {
        if (!left.elementType || !right.elementType) {
            return simpleType(StaticType::Array);
        }
        std::optional<TypeInfo> element = mergeArrayElementTypes(*left.elementType, *right.elementType);
        if (!element) {
            return std::nullopt;
        }
        return arrayType(std::move(*element));
    }

    if (compatible(left, right) && compatible(right, left)) {
        return left;
    }

    return std::nullopt;
}
