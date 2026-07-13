#include "TypeUtils.hpp"

#include <cassert>

int main()
{
    const TypeInfo t = typeParameterType("T");
    assert(typeInfoName(t) == "T");
    assert(compatible(t, typeParameterType("T")));
    assert(!compatible(t, typeParameterType("U")));

    const TypeInfo identity = functionType({t}, t, {"T"});
    assert(typeInfoName(identity) == "fun<T>(T): T");

    const TypeInfo nested = arrayType(t);
    assert(compatible(nested, arrayType(typeParameterType("T"))));
    assert(!compatible(nested, arrayType(simpleType(StaticType::Number))));
    return 0;
}
