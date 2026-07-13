#include "FrontendSession.hpp"
#include "IRCompiler.hpp"
#include "TypeChecker.hpp"

#include <cassert>
#include <sstream>

int main()
{
    std::istringstream input("print 1 / 0;\n");
    FrontendSession frontend;
    Program program = frontend.loadStdin(input);
    TypeChecker checker;
    const ResolvedNames& resolved = checker.check(program);
    IRCompiler compiler;
    IRProgram ir = compiler.compile(program, resolved);

    assert(ir.sources().size() == 1);
    const auto& divide = ir.instructions().at(2);
    assert(divide.op == IROp::Divide);
    assert(divide.span.has_value());
    assert(divide.span->source == 0);
    assert(divide.span->line == 1);
    assert(divide.span->column == 7);
    return 0;
}
