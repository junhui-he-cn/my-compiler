#include "NativeStdlib.hpp"

#include <array>

namespace {

constexpr std::array<NativeFunctionSignature, 9> kNativeFunctions{{
    {"push", 2, NativeFunctionKind::Push},
    {"pop", 1, NativeFunctionKind::Pop},
    {"floor", 1, NativeFunctionKind::Floor},
    {"ceil", 1, NativeFunctionKind::Ceil},
    {"sqrt", 1, NativeFunctionKind::Sqrt},
    {"str", 1, NativeFunctionKind::Str},
    {"substr", 3, NativeFunctionKind::Substr},
    {"charAt", 2, NativeFunctionKind::CharAt},
    {"typeOf", 1, NativeFunctionKind::TypeOf},
}};

} // namespace

const NativeFunctionSignature* findNativeStdlibFunction(const std::string& name)
{
    for (const NativeFunctionSignature& function : kNativeFunctions) {
        if (name == function.name) {
            return &function;
        }
    }
    return nullptr;
}

bool isNativeStdlibName(const std::string& name)
{
    return findNativeStdlibFunction(name) != nullptr;
}

std::optional<std::size_t> nativeStdlibArity(const std::string& name)
{
    const NativeFunctionSignature* function = findNativeStdlibFunction(name);
    if (!function) {
        return std::nullopt;
    }
    return function->arity;
}
