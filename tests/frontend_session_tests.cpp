#include "Ast.hpp"
#include "FrontendSession.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "compiler_frontend_session_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "nested");

    std::ofstream(root / "shared.cd") << "let value = 1;\nexport value;\n";
    std::ofstream(root / "input.cd")
        << "import \"./shared.cd\";\n"
           "import \"./nested/../shared.cd\";\n"
           "print value;\n";

    FrontendSession session;
    Program program = session.loadFiles({(root / "input.cd").string()});

    assert(session.moduleCount() == 2);
    assert(program.statements.size() == 2);
    const auto* entry = dynamic_cast<const ModuleStmt*>(program.statements[1].get());
    assert(entry != nullptr);
    const auto* first = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* second = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(first != nullptr && second != nullptr);
    assert(first->resolvedModuleId == second->resolvedModuleId);

    std::filesystem::remove_all(root);
}
