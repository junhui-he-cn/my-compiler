#include "ModuleProgram.hpp"

#include "Lexer.hpp"
#include "Parser.hpp"

#include <unordered_map>
#include <utility>

ModuleAssemblyError::ModuleAssemblyError(std::string message)
    : DiagnosticError(DiagnosticKind::Compile, std::move(message))
{
}

namespace {

ImportStmt* asImport(StmtPtr& statement)
{
    return dynamic_cast<ImportStmt*>(statement.get());
}

std::vector<StmtPtr> parseUnit(const SourceUnit& unit)
{
    Lexer lexer(unit.source);
    Parser parser(lexer.scanTokens());
    Program parsed = parser.parse();
    return std::move(parsed.statements);
}

void patchImports(
    const SourceUnit& unit,
    std::vector<StmtPtr>& statements,
    const std::unordered_map<std::string, std::size_t>& canonicalToId)
{
    std::size_t importIndex = 0;
    for (StmtPtr& statement : statements) {
        ImportStmt* import = asImport(statement);
        if (!import) {
            continue;
        }
        if (importIndex >= unit.imports.size()) {
            continue;
        }
        const SourceImport& edge = unit.imports[importIndex++];
        const auto found = canonicalToId.find(edge.canonicalPath);
        if (found == canonicalToId.end()) {
            throw ModuleAssemblyError("internal error: unresolved import " + edge.requestedPath);
        }
        import->resolvedModuleId = found->second;
    }
}

} // namespace

Program buildModuleProgram(const SourceLoadResult& loadResult)
{
    std::unordered_map<std::string, std::size_t> canonicalToId;
    for (const SourceUnit& unit : loadResult.units) {
        canonicalToId.emplace(unit.canonicalPath, unit.id);
    }

    Program program;
    for (const SourceUnit& unit : loadResult.units) {
        std::vector<StmtPtr> statements = parseUnit(unit);
        patchImports(unit, statements, canonicalToId);
        program.statements.push_back(std::make_unique<ModuleStmt>(
            unit.id,
            unit.path,
            std::move(statements),
            unit.isEntry));
    }
    return program;
}
