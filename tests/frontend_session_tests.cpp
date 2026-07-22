#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "FrontendSession.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string pathString(const fs::path& path)
{
    return path.lexically_normal().generic_string();
}

void writeFile(const fs::path& path, const std::string& source)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    output << source;
}

const ModuleStmt* moduleByPath(const Program& program, const fs::path& path)
{
    const std::string expected = pathString(path);
    for (const StmtPtr& statement : program.statements) {
        const auto* module = dynamic_cast<const ModuleStmt*>(statement.get());
        if (module && module->path == expected) {
            return module;
        }
    }
    assert(false && "expected module path not found");
    return nullptr;
}

void expectImportError(const std::function<void()>& action, const std::string& expectedMessage)
{
    try {
        action();
    } catch (const DiagnosticError& error) {
        assert(error.kind() == DiagnosticKind::Import);
        assert(error.message() == expectedMessage);
        return;
    }
    assert(false && "expected import error");
}

void test_canonical_duplicate_import_spellings_are_deduplicated(const fs::path& root)
{
    fs::remove_all(root);
    fs::create_directories(root / "nested");

    writeFile(root / "shared.cd", "let value = 1;\nexport value;\n");
    writeFile(
        root / "input.cd",
        "import \"./shared.cd\";\n"
        "import \"./nested/../shared.cd\";\n"
        "print value;\n");

    FrontendSession session;
    Program program = session.loadFiles({(root / "input.cd").string()});

    assert(session.moduleCount() == 2);
    assert(program.statements.size() == 2);
    const auto* entry = moduleByPath(program, root / "input.cd");
    const auto* first = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* second = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(first != nullptr && second != nullptr);
    assert(first->resolvedModuleId == second->resolvedModuleId);
}

void test_search_path_resolves_extensionless_import_and_reexport(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(search / "lib.cd", "let value = 7;\nexport value;\n");
    writeFile(app / "api.cd", "export value from \"lib\";\n");
    writeFile(
        app / "input.cd",
        "import \"api\";\n"
        "import \"lib\";\n"
        "print value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    Program program = session.loadFiles({(app / "input.cd").string()});

    assert(session.moduleCount() == 3);
    const auto* lib = moduleByPath(program, search / "lib.cd");
    const auto* api = moduleByPath(program, app / "api.cd");
    const auto* entry = moduleByPath(program, app / "input.cd");

    const auto* reExport = dynamic_cast<const ExportStmt*>(api->statements[0].get());
    assert(reExport != nullptr);
    assert(reExport->resolvedModuleId == lib->moduleId);

    const auto* apiImport = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* libImport = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(apiImport != nullptr && libImport != nullptr);
    assert(apiImport->resolvedModuleId == api->moduleId);
    assert(libImport->resolvedModuleId == lib->moduleId);
}

void test_importing_file_directory_precedes_search_path(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(app / "math.cd", "let value = \"local\";\nexport value;\n");
    writeFile(search / "math.cd", "let value = \"search\";\nexport value;\n");
    writeFile(app / "input.cd", "import \"math\";\nprint value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    Program program = session.loadFiles({(app / "input.cd").string()});

    assert(session.moduleCount() == 2);
    const auto* localMath = moduleByPath(program, app / "math.cd");
    const auto* entry = moduleByPath(program, app / "input.cd");
    const auto* import = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    assert(import != nullptr);
    assert(import->resolvedModuleId == localMath->moduleId);
}

void test_explicit_relative_import_does_not_use_search_path(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(search / "missing.cd", "let value = \"search\";\nexport value;\n");
    writeFile(app / "input.cd", "import \"./missing\";\nprint value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    expectImportError(
        [&]() { session.loadFiles({(app / "input.cd").string()}); },
        "failed to open import: " + pathString(app / "missing"));
}

void test_direct_inputs_preserve_source_spans(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path first = root / "first.cd";
    const fs::path second = root / "second.cd";
    writeFile(first, "print 1;\n");
    writeFile(second, "print 2;\n");

    FrontendSession session;
    Program program = session.loadFiles({first.string(), second.string()});

    assert(program.sources.size() == 2);
    assert(program.sources[0].path.find("first.cd") != std::string::npos);
    assert(program.sources[1].path.find("second.cd") != std::string::npos);
    const auto* firstPrint = dynamic_cast<const PrintStmt*>(program.statements.front().get());
    assert(firstPrint != nullptr);
    assert(firstPrint->span.has_value());
    assert(firstPrint->span->source == 0);
    assert(firstPrint->span->line == 1);
}

void test_direct_diagnostics_keep_source_ranges(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path first = root / "first.cd";
    const fs::path second = root / "second.cd";
    writeFile(first, "print 1;\n");
    writeFile(second, "print ;\n");

    FrontendSession session;
    try {
        (void)session.loadFiles({first.string(), second.string()});
    } catch (const FileDiagnosticErrorList& errors) {
        assert(errors.errors().size() == 1);
        const FileDiagnosticError& error = errors.errors().front();
        assert(error.sourceContext().path.find("second.cd") != std::string::npos);
        assert(error.range().has_value());
        assert(error.range()->source == SourceFileId{1});
        assert(error.range()->start <= error.range()->end);
        assert(error.range()->end <= error.sourceContext().source.size());
        return;
    }
    assert(false && "expected direct multi-file parse diagnostic");
}

} // namespace

int main()
{
    const fs::path root = fs::temp_directory_path() / "compiler_frontend_session_test";

    test_canonical_duplicate_import_spellings_are_deduplicated(root / "duplicates");
    test_search_path_resolves_extensionless_import_and_reexport(root / "search_reexport");
    test_importing_file_directory_precedes_search_path(root / "precedence");
    test_explicit_relative_import_does_not_use_search_path(root / "explicit_no_fallback");
    test_direct_inputs_preserve_source_spans(root / "direct_sources");
    test_direct_diagnostics_keep_source_ranges(root / "direct_diagnostics");

    fs::remove_all(root);
}
