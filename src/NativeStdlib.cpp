#include "NativeStdlib.hpp"

#include <array>

namespace {

constexpr std::array<NativeFunctionSignature, 2> kNativeFunctions{{
    {"push", 2},
    {"pop", 1},
}};

} // namespace

bool isNativeStdlibName(const std::string& name)
{
    return nativeStdlibArity(name).has_value();
}

std::optional<std::size_t> nativeStdlibArity(const std::string& name)
{
    for (const NativeFunctionSignature& function : kNativeFunctions) {
        if (name == function.name) {
            return function.arity;
        }
    }
    return std::nullopt;
}
