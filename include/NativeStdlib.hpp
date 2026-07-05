#pragma once

#include <cstddef>
#include <optional>
#include <string>

struct NativeFunctionSignature {
    const char* name;
    std::size_t arity;
};

bool isNativeStdlibName(const std::string& name);
std::optional<std::size_t> nativeStdlibArity(const std::string& name);
