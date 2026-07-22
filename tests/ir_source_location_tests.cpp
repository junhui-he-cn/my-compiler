#include "FrontendSession.hpp"
#include "IRCompiler.hpp"
#include "TypeChecker.hpp"

#include <cassert>
#include <sstream>

namespace {

void test_snapshot_identity_metadata()
{
    const std::string source =
        "let x = 1;\n"
        "{ let x = 2; print x; x = 3; }\n"
        "x = 4;\n";
    std::istringstream input(source);
    FrontendSession frontend;
    Program program = frontend.loadStdin(input);

    assert(program.sources.size() == 1);
    assert(program.sources[0].id == SourceFileId{0});
    for (const Token& token : frontend.displayTokens()) {
        assert(token.range.has_value());
        assert(isValidSourceRange(*token.range, program.sources));
    }

    const auto* outer = dynamic_cast<const LetStmt*>(program.statements[0].get());
    const auto* block = dynamic_cast<const BlockStmt*>(program.statements[1].get());
    const auto* trailing = dynamic_cast<const ExpressionStmt*>(program.statements[2].get());
    assert(outer != nullptr && block != nullptr && trailing != nullptr);
    assert(outer->range.has_value() && outer->syntaxNodeId.has_value());
    assert(block->range.has_value() && block->syntaxNodeId.has_value());
    assert(trailing->range.has_value() && trailing->syntaxNodeId.has_value());
    assert(isValidSourceRange(*outer->range, program.sources));
    assert(isValidSourceRange(*block->range, program.sources));
    assert(isValidSourceRange(*trailing->range, program.sources));
    assert(outer->syntaxNodeId != block->syntaxNodeId);

    const auto* inner = dynamic_cast<const LetStmt*>(block->statements[0].get());
    const auto* print = dynamic_cast<const PrintStmt*>(block->statements[1].get());
    const auto* assignmentStatement = dynamic_cast<const ExpressionStmt*>(block->statements[2].get());
    assert(inner != nullptr && print != nullptr && assignmentStatement != nullptr);
    const auto* read = dynamic_cast<const VariableExpr*>(print->expression.get());
    const auto* assignment = dynamic_cast<const AssignExpr*>(assignmentStatement->expression.get());
    assert(read != nullptr && assignment != nullptr);

    TypeChecker checker;
    const ResolvedNames& resolved = checker.check(program);
    const BindingId outerBinding = resolved.letBindingId(*outer);
    const BindingId innerBinding = resolved.letBindingId(*inner);
    assert(outerBinding != innerBinding);
    assert(resolved.declarationId(*outer) == resolved.binding(outerBinding).declarationId);
    assert(resolved.symbolId(*outer) == resolved.binding(outerBinding).symbolId);
    assert(resolved.declarationId(*inner) == resolved.binding(innerBinding).declarationId);
    assert(resolved.symbolId(*inner) == resolved.binding(innerBinding).symbolId);
    assert(resolved.variableBindingId(*read) == innerBinding);
    assert(resolved.assignmentBindingId(*assignment) == innerBinding);
    assert(resolved.hasScope(*block));
    assert(resolved.binding(outerBinding).scopeId != resolved.binding(innerBinding).scopeId);
    assert(resolved.bindingCount() >= 2);
    assert(resolved.bindingShadowMismatchCount() == 0);
}

} // namespace

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

    test_snapshot_identity_metadata();
    return 0;
}
