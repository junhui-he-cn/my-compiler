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

    const TypeInfo stringNumberMap = mapType(
        simpleType(StaticType::String),
        simpleType(StaticType::Number));
    assert(typeInfoName(stringNumberMap) == "map<string, number>");
    assert(compatible(
        stringNumberMap,
        mapType(simpleType(StaticType::String), simpleType(StaticType::Number))));
    assert(!compatible(
        stringNumberMap,
        mapType(simpleType(StaticType::Number), simpleType(StaticType::Number))));
    assert(compatible(
        mapType(typeParameterType("K"), arrayType(typeParameterType("V"))),
        mapType(typeParameterType("K"), arrayType(typeParameterType("V")))));
    return 0;
}
