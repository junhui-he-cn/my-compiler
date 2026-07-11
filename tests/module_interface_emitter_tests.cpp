#include "ModuleInterface.hpp"
#include "ModuleInterfaceEmitter.hpp"
#include "TypeUtils.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectEqual(const std::string& name, const std::string& actual, const std::string& expected)
{
    if (actual == expected) {
        return;
    }
    ++failures;
    std::cerr << "FAIL " << name << "\nexpected:\n" << expected << "\nactual:\n" << actual << "\n";
}

std::string emit(const std::vector<ModuleInterface>& interfaces)
{
    std::ostringstream out;
    writeModuleInterfaceText(out, interfaces);
    return out.str();
}

void testEmptyAndSortedModules()
{
    std::vector<ModuleInterface> interfaces;

    ModuleInterface later;
    later.moduleId = 2;
    later.path = "later.cd";

    ModuleInterface entry;
    entry.moduleId = 0;
    entry.path = "main.cd";
    entry.isEntry = true;

    interfaces.push_back(later);
    interfaces.push_back(entry);

    expectEqual(
        "empty modules sorted by id",
        emit(interfaces),
        "module 0 entry \"main.cd\"\n"
        "\n"
        "module 2 \"later.cd\"\n");
}

void testValuesStructsFieldsMethodsAndTypes()
{
    ModuleInterface module;
    module.moduleId = 0;
    module.path = "main.cd";
    module.isEntry = true;

    module.values.push_back(ModuleInterfaceValue{"zeta", nullableType(namedStructType("Point"))});
    module.values.push_back(ModuleInterfaceValue{
        "make",
        functionType(
            std::vector<TypeInfo>{simpleType(StaticType::Number), arrayType(nullableType(simpleType(StaticType::String)))},
            namedStructType("Point"))});
    module.values.push_back(ModuleInterfaceValue{"dynamic", unknownType()});

    ModuleInterfaceStruct point;
    point.name = "Point";
    point.fields.push_back(ModuleInterfaceField{"y", simpleType(StaticType::Number)});
    point.fields.push_back(ModuleInterfaceField{"x", nullableType(simpleType(StaticType::Number))});
    point.methods.push_back(ModuleInterfaceMethod{"translate", {simpleType(StaticType::Number), simpleType(StaticType::Number)}, namedStructType("Point")});
    point.methods.push_back(ModuleInterfaceMethod{"length", {}, simpleType(StaticType::Number)});

    ModuleInterfaceStruct box;
    box.name = "Box";
    box.fields.push_back(ModuleInterfaceField{"items", arrayType(namedStructType("Point"))});

    module.structs.push_back(point);
    module.structs.push_back(box);

    expectEqual(
        "exports sorted and fields preserved",
        emit(std::vector<ModuleInterface>{module}),
        "module 0 entry \"main.cd\"\n"
        "  export value dynamic: unknown\n"
        "  export value make: fun(number, [string?]): Point\n"
        "  export value zeta: Point?\n"
        "  export struct Box\n"
        "    field items: [Point]\n"
        "  export struct Point\n"
        "    field y: number\n"
        "    field x: number?\n"
        "    method length(): number\n"
        "    method translate(number, number): Point\n");
}

} // namespace

int main()
{
    testEmptyAndSortedModules();
    testValuesStructsFieldsMethodsAndTypes();
    return failures == 0 ? 0 : 1;
}
