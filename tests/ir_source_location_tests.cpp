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

void test_declaration_index()
{
    const std::string source =
        "struct Box { value: number }\n"
        "enum Result { Ok(number), Empty }\n"
        "enum Choice { Left(number), Right(number) }\n"
        "impl Box {\n"
        "  fun bump(delta: number): number {\n"
        "    let local = delta;\n"
        "    local += 1;\n"
        "    local = local + 1;\n"
        "    return local;\n"
        "  }\n"
        "}\n"
        "fun use(value: number): number {\n"
        "  let local = value;\n"
        "  local += 1;\n"
        "  local = local + 1;\n"
        "  return local;\n"
        "}\n"
        "fun choose(value: Choice): number {\n"
        "  return match value {\n"
        "    Choice.Left(number) | Choice.Right(number) => number,\n"
        "  };\n"
        "}\n"
        "let x = 1;\n"
        "let result = Result.Ok(1);\n"
        "let box = Box { value: 1 };\n"
        "print box.bump(1);\n"
        "{\n"
        "  let x = 2;\n"
        "  print x;\n"
        "  x = 3;\n"
        "  x += 1;\n"
        "}\n"
        "print use(x);\n";
    std::istringstream input(source);
    FrontendSession frontend;
    Program program = frontend.loadStdin(input);

    const auto* structDecl = dynamic_cast<const StructDeclStmt*>(program.statements[0].get());
    const auto* enumDecl = dynamic_cast<const EnumDeclStmt*>(program.statements[1].get());
    const auto* choiceDecl = dynamic_cast<const EnumDeclStmt*>(program.statements[2].get());
    const auto* impl = dynamic_cast<const ImplStmt*>(program.statements[3].get());
    const auto* function = dynamic_cast<const FunctionStmt*>(program.statements[4].get());
    const auto* choose = dynamic_cast<const FunctionStmt*>(program.statements[5].get());
    const auto* outer = dynamic_cast<const LetStmt*>(program.statements[6].get());
    const auto* result = dynamic_cast<const LetStmt*>(program.statements[7].get());
    const auto* box = dynamic_cast<const LetStmt*>(program.statements[8].get());
    const auto* block = dynamic_cast<const BlockStmt*>(program.statements[10].get());
    assert(structDecl != nullptr && enumDecl != nullptr && choiceDecl != nullptr);
    assert(impl != nullptr && function != nullptr && choose != nullptr);
    assert(outer != nullptr && result != nullptr && box != nullptr && block != nullptr);
    assert(impl->methods.size() == 1);
    const MethodDecl& method = impl->methods.front();

    TypeChecker checker;
    checker.check(program);
    const DeclarationIndex& index = checker.declarationIndex();
    assert(checker.declarationIndexMismatchCount() == 0);

    const DeclarationRecord* structRecord = index.declaration(*structDecl);
    const DeclarationRecord* enumRecord = index.declaration(*enumDecl);
    const DeclarationRecord* choiceRecord = index.declaration(*choiceDecl);
    const DeclarationRecord* methodRecord = index.declaration(method);
    const DeclarationRecord* functionRecord = index.declaration(*function);
    const DeclarationRecord* chooseRecord = index.declaration(*choose);
    const DeclarationRecord* outerRecord = index.declaration(*outer);
    const auto* inner = dynamic_cast<const LetStmt*>(block->statements[0].get());
    assert(structRecord != nullptr && enumRecord != nullptr && choiceRecord != nullptr);
    assert(methodRecord != nullptr && functionRecord != nullptr && chooseRecord != nullptr);
    const DeclarationRecord* innerRecord = inner ? index.declaration(*inner) : nullptr;
    assert(outerRecord != nullptr && innerRecord != nullptr);
    assert(structRecord->kind == DeclarationKind::Struct);
    assert(enumRecord->kind == DeclarationKind::Enum);
    assert(choiceRecord->kind == DeclarationKind::Enum);
    assert(methodRecord->kind == DeclarationKind::Method);
    assert(methodRecord->ownerType == "Box");
    assert(methodRecord->parameters.size() == 1);
    assert(functionRecord->kind == DeclarationKind::Function);
    assert(functionRecord->parameters.size() == 1);
    assert(chooseRecord->kind == DeclarationKind::Function);
    assert(outerRecord->kind == DeclarationKind::Variable);
    assert(outerRecord->range.has_value());
    assert(isValidSourceRange(*outerRecord->range, program.sources));
    assert(index.declaration(function->parameters.front())->kind == DeclarationKind::Parameter);
    assert(index.declaration(method.parameters.front())->kind == DeclarationKind::Parameter);

    const std::optional<ScopeId> rootScope = index.scopes().empty()
        ? std::nullopt
        : std::optional<ScopeId>(index.scopes().front().id);
    const std::optional<ScopeId> blockScope = index.scopeFor(*block);
    const std::optional<ScopeId> functionScope = index.scopeFor(*function);
    assert(rootScope.has_value() && blockScope.has_value() && functionScope.has_value());
    assert(index.scope(*blockScope) != nullptr);
    assert(index.scope(*blockScope)->parent == rootScope);
    assert(index.scope(*functionScope)->parent == rootScope);
    assert(index.lookup(*rootScope, "x") == outerRecord->declarationId);
    const DeclarationId innerId = innerRecord->declarationId;
    assert(index.lookup(*blockScope, "x") == innerId);

    const auto* read = dynamic_cast<const VariableExpr*>(
        dynamic_cast<const PrintStmt*>(block->statements[1].get())->expression.get());
    const auto* assignment = dynamic_cast<const AssignExpr*>(
        dynamic_cast<const ExpressionStmt*>(block->statements[2].get())->expression.get());
    const auto* compound = dynamic_cast<const CompoundAssignExpr*>(
        dynamic_cast<const ExpressionStmt*>(block->statements[3].get())->expression.get());
    assert(read != nullptr && assignment != nullptr && compound != nullptr);
    assert(index.variableReference(*read)->declarationId == innerId);
    assert(index.assignmentReference(*assignment)->declarationId == innerId);
    assert(index.compoundAssignmentReference(*compound)->declarationId == innerId);

    const auto* resultConstructor = dynamic_cast<const MemberCallExpr*>(result->initializer.get());
    const auto* enumQualifier = resultConstructor
        ? dynamic_cast<const VariableExpr*>(resultConstructor->receiver.get())
        : nullptr;
    assert(resultConstructor != nullptr && enumQualifier != nullptr);
    assert(!index.variableReference(*enumQualifier).has_value());

    const auto* callStatement = dynamic_cast<const PrintStmt*>(program.statements[9].get());
    const auto* methodCall = callStatement
        ? dynamic_cast<const MemberCallExpr*>(callStatement->expression.get())
        : nullptr;
    const auto* methodReceiver = methodCall
        ? dynamic_cast<const VariableExpr*>(methodCall->receiver.get())
        : nullptr;
    assert(methodCall != nullptr && methodReceiver != nullptr);
    assert(index.variableReference(*methodReceiver)->declarationId == index.declaration(*box)->declarationId);

    const auto* chooseMatch = dynamic_cast<const MatchExpr*>(
        dynamic_cast<const ReturnStmt*>(choose->body.front().get())->value.get());
    const auto* orPattern = chooseMatch && !chooseMatch->arms.empty()
        ? dynamic_cast<const OrPattern*>(chooseMatch->arms.front().pattern.get())
        : nullptr;
    assert(orPattern != nullptr && orPattern->alternatives.size() == 2);
    const auto* firstPattern = dynamic_cast<const VariantPattern*>(orPattern->alternatives[0].get());
    const auto* secondPattern = dynamic_cast<const VariantPattern*>(orPattern->alternatives[1].get());
    assert(firstPattern != nullptr && secondPattern != nullptr);
    const auto* firstBinding = dynamic_cast<const VariablePattern*>(firstPattern->arguments[0].get());
    const auto* secondBinding = dynamic_cast<const VariablePattern*>(secondPattern->arguments[0].get());
    assert(firstBinding != nullptr && secondBinding != nullptr);
    assert(index.declaration(*firstBinding) != nullptr);
    assert(index.declaration(*firstBinding)->declarationId
        == index.declaration(*secondBinding)->declarationId);
}

void test_declaration_index_module_metadata()
{
    Token importKeyword{TokenType::Import, "import", 1, 1};
    Token importPath{TokenType::String, "\"lib.cd\"", 1, 8};
    Token alias{TokenType::Identifier, "lib", 1, 18};
    auto import = std::make_unique<ImportStmt>(
        importKeyword, importPath, std::optional<Token>(alias));
    import->resolvedModuleId = 7;

    Token exportKeyword{TokenType::Export, "export", 2, 1};
    Token exportedName{TokenType::Identifier, "value", 2, 8};
    Token sourcePath{TokenType::String, "\"base.cd\"", 2, 19};
    auto exportStmt = std::make_unique<ExportStmt>(
        exportKeyword,
        std::vector<Token>{exportedName},
        std::optional<Token>(sourcePath));
    exportStmt->resolvedModuleId = 11;

    Program program;
    program.statements.push_back(std::move(import));
    program.statements.push_back(std::move(exportStmt));
    const DeclarationIndex index = DeclarationIndex::collect(program);

    assert(index.imports().size() == 1);
    assert(index.imports().front().resolvedModuleId == 7);
    assert(index.imports().front().alias == std::optional<std::string>("lib"));
    assert(index.exports().size() == 1);
    assert(index.exports().front().resolvedModuleId == 11);
    assert(index.exports().front().names == std::vector<std::string>{"value"});
    assert(index.exports().front().sourcePath == std::optional<std::string>("\"base.cd\""));
    assert(index.scopes().size() == 1);
    const std::optional<DeclarationId> aliasId = index.lookup(index.scopes().front().id, "lib");
    assert(aliasId.has_value());
    assert(index.declaration(*aliasId)->kind == DeclarationKind::NamespaceAlias);
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
    test_declaration_index();
    test_declaration_index_module_metadata();
    return 0;
}
