#pragma once

#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "Parser.hpp"
#include "Token.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class FrontendSession {
public:
    void setImportSearchPaths(std::vector<std::string> paths);

    Program loadStdin(std::istream& input);
    Program loadFiles(const std::vector<std::string>& paths);

    std::vector<Token> displayTokens() const;
    const std::string& sourceForDiagnostics() const;
    std::optional<FileDiagnosticError> remapDirectDiagnostic(const DiagnosticError& error) const;
    std::optional<FileDiagnosticErrorList> remapDirectDiagnostics(const ParseErrorList& errors) const;
    std::size_t moduleCount() const;

private:
    struct ParsedUnit {
        std::size_t id = 0;
        std::string path;
        std::string canonicalPath;
        std::string source;
        std::vector<Token> tokens;
        std::vector<StmtPtr> statements;
        bool isEntry = false;
    };

    struct DirectInput {
        std::string path;
        std::string canonicalPath;
        std::string source;
    };

    struct ImportResolution {
        std::filesystem::path path;
        std::vector<std::string> triedDisplayPaths;
    };

    void reset();
    std::size_t loadFile(const std::string& path, bool isImport, bool isEntry, bool fileDiagnostics);
    ImportResolution resolveImportPath(const std::filesystem::path& importingPath, const Token& pathToken) const;
    Program assembleProgram();
    void rebuildCombinedSource();

    std::vector<ParsedUnit> units_;
    std::unordered_map<std::string, std::size_t> canonicalToUnitId_;
    std::vector<std::string> loadingStack_;
    std::vector<std::size_t> entryUnitIds_;
    std::vector<DirectInput> directInputs_;
    std::vector<Token> directDisplayTokens_;
    std::vector<std::filesystem::path> importSearchPaths_;
    std::string combinedSource_;
    bool hasImports_ = false;
};
