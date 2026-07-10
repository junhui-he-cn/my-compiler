#include "FrontendSession.hpp"

#include "Diagnostic.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

std::string readAll(std::istream& input)
{
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::filesystem::path normalizedExistingPath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical.lexically_normal();
    }
    return std::filesystem::absolute(path).lexically_normal();
}

std::string pathString(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string displayCycle(const std::vector<std::string>& stack, const std::string& repeated)
{
    const auto found = std::find(stack.begin(), stack.end(), repeated);
    std::ostringstream output;
    if (found == stack.end()) {
        output << repeated << " -> " << repeated;
        return output.str();
    }

    for (auto current = found; current != stack.end(); ++current) {
        if (current != found) {
            output << " -> ";
        }
        output << *current;
    }
    output << " -> " << repeated;
    return output.str();
}

void appendWithNewlineSeparation(std::string& output, const std::string& source)
{
    if (!output.empty() && output.back() != '\n') {
        output.push_back('\n');
    }
    output += source;
    if (!output.empty() && output.back() != '\n') {
        output.push_back('\n');
    }
}

bool hasImportToken(const std::vector<Token>& tokens)
{
    return std::any_of(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.type == TokenType::Import;
    });
}

bool statementLoadsSource(const Stmt& statement)
{
    if (dynamic_cast<const ImportStmt*>(&statement)) {
        return true;
    }
    const auto* exportStmt = dynamic_cast<const ExportStmt*>(&statement);
    return exportStmt && exportStmt->sourcePath.has_value();
}

bool programLoadsSource(const Program& program)
{
    return std::any_of(program.statements.begin(), program.statements.end(), [](const StmtPtr& statement) {
        return statementLoadsSource(*statement);
    });
}

std::string importPath(const Token& token)
{
    if (token.lexeme.size() >= 2 && token.lexeme.front() == '"' && token.lexeme.back() == '"') {
        return token.lexeme.substr(1, token.lexeme.size() - 2);
    }
    return token.lexeme;
}

struct ParsedSource {
    std::vector<Token> tokens;
    std::vector<StmtPtr> statements;
};

ParsedSource parseSource(
    const std::string& path,
    const std::string& source,
    bool pathlessDiagnostics)
{
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scanTokens();
        Parser parser(tokens);
        Program program = parser.parse();
        return ParsedSource{std::move(tokens), std::move(program.statements)};
    } catch (const FileDiagnosticError&) {
        throw;
    } catch (const DiagnosticError& error) {
        if (error.location()) {
            throw FileDiagnosticError(
                error,
                DiagnosticSourceContext{path, source, pathlessDiagnostics});
        }
        throw;
    }
}

int lineAtEnd(const std::string& source)
{
    int line = 1;
    for (const char ch : source) {
        if (ch == '\n') {
            ++line;
        }
    }
    return line;
}

Token endOfFileToken(const std::string& source)
{
    int line = 1;
    int column = 1;
    for (const char ch : source) {
        if (ch == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return Token{TokenType::EndOfFile, "", line, column};
}

std::size_t sourceLineSpan(const std::string& source)
{
    if (source.empty()) {
        return 0;
    }

    std::size_t lines = 1;
    for (const char ch : source) {
        if (ch == '\n') {
            ++lines;
        }
    }
    if (source.back() == '\n') {
        --lines;
    }
    return lines;
}

} // namespace

void FrontendSession::reset()
{
    units_.clear();
    canonicalToUnitId_.clear();
    loadingStack_.clear();
    entryUnitIds_.clear();
    directInputs_.clear();
    directDisplayTokens_.clear();
    combinedSource_.clear();
    hasImports_ = false;
}

Program FrontendSession::loadStdin(std::istream& input)
{
    reset();

    std::string source = readAll(input);
    ParsedSource parsed = parseSource("<stdin>", source, true);
    if (hasImportToken(parsed.tokens)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }

    Program parsedProgram;
    parsedProgram.statements = std::move(parsed.statements);
    if (programLoadsSource(parsedProgram)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }

    units_.push_back(ParsedUnit{
        0,
        "<stdin>",
        "<stdin>",
        std::move(source),
        std::move(parsed.tokens),
        std::move(parsedProgram.statements),
        true,
    });
    entryUnitIds_.push_back(0);
    rebuildCombinedSource();
    return assembleProgram();
}

Program FrontendSession::loadFiles(const std::vector<std::string>& paths)
{
    reset();

    std::unordered_map<std::string, std::size_t> directInputIds;
    bool hasImports = false;
    for (const std::string& path : paths) {
        const std::filesystem::path requestedPath(path);
        const std::filesystem::path normalizedPath = normalizedExistingPath(requestedPath);
        const std::string canonicalPath = pathString(normalizedPath);
        if (directInputIds.find(canonicalPath) != directInputIds.end()) {
            continue;
        }

        const std::string displayPath = pathString(requestedPath);
        std::ifstream input(requestedPath);
        if (!input) {
            throw std::runtime_error("failed to open input file: " + displayPath);
        }

        std::string source = readAll(input);
        directInputIds.emplace(canonicalPath, directInputs_.size());
        directInputs_.push_back(DirectInput{
            displayPath,
            canonicalPath,
            std::move(source),
        });
    }

    for (const DirectInput& input : directInputs_) {
        appendWithNewlineSeparation(combinedSource_, input.source);
    }
    Program directProgram;
    try {
        Lexer lexer(combinedSource_);
        directDisplayTokens_ = lexer.scanTokensUntil(TokenType::Import);
        hasImports = !directDisplayTokens_.empty()
            && directDisplayTokens_.back().type == TokenType::Import;
        if (!hasImports) {
            Parser parser(directDisplayTokens_);
            directProgram = parser.parse();
            hasImports = programLoadsSource(directProgram);
        }
    } catch (const DiagnosticError& error) {
        if (const std::optional<FileDiagnosticError> remapped = remapDirectDiagnostic(error)) {
            throw *remapped;
        }
        throw;
    }

    if (!hasImports) {
        return directProgram;
    }

    directInputs_.clear();
    directDisplayTokens_.clear();
    for (const std::string& path : paths) {
        const std::size_t id = loadFile(path, false, true, true);
        if (std::find(entryUnitIds_.begin(), entryUnitIds_.end(), id) == entryUnitIds_.end()) {
            entryUnitIds_.push_back(id);
        }
    }
    rebuildCombinedSource();
    return assembleProgram();
}

std::size_t FrontendSession::loadFile(
    const std::string& path,
    bool isImport,
    bool isEntry,
    bool fileDiagnostics)
{
    const std::filesystem::path requestedPath(path);
    const std::filesystem::path normalizedPath = normalizedExistingPath(requestedPath);
    const std::string canonicalPath = pathString(normalizedPath);
    const std::string displayPath = pathString(requestedPath);

    if (std::find(loadingStack_.begin(), loadingStack_.end(), canonicalPath) != loadingStack_.end()) {
        throw DiagnosticError(
            DiagnosticKind::Import,
            "import cycle detected: " + displayCycle(loadingStack_, canonicalPath));
    }

    const auto loaded = canonicalToUnitId_.find(canonicalPath);
    if (loaded != canonicalToUnitId_.end()) {
        if (isEntry) {
            units_[loaded->second].isEntry = true;
        }
        return loaded->second;
    }

    std::ifstream input(requestedPath);
    if (!input) {
        if (isImport) {
            throw DiagnosticError(DiagnosticKind::Import, "failed to open import: " + displayPath);
        }
        throw std::runtime_error("failed to open input file: " + displayPath);
    }

    loadingStack_.push_back(canonicalPath);
    try {
        std::string source = readAll(input);
        std::vector<Token> tokens;
        try {
            Lexer lexer(source);
            tokens = lexer.scanTokens();
        } catch (const DiagnosticError& error) {
            if (error.location()) {
                throw FileDiagnosticError(
                    error,
                    DiagnosticSourceContext{displayPath, source, !fileDiagnostics && !isImport});
            }
            throw;
        }
        const bool thisUnitHasImport = hasImportToken(tokens);
        ParsedSource parsed;
        try {
            Parser parser(tokens);
            Program program = parser.parse();
            parsed = ParsedSource{std::move(tokens), std::move(program.statements)};
        } catch (const FileDiagnosticError&) {
            throw;
        } catch (const DiagnosticError& error) {
            if (error.location()) {
                throw FileDiagnosticError(
                    error,
                    DiagnosticSourceContext{
                        displayPath,
                        source,
                        !fileDiagnostics && !isImport && !thisUnitHasImport,
                    });
            }
            throw;
        }

        ParsedUnit unit{
            0,
            displayPath,
            canonicalPath,
            std::move(source),
            std::move(parsed.tokens),
            std::move(parsed.statements),
            isEntry,
        };

        for (StmtPtr& statement : unit.statements) {
            if (auto* import = dynamic_cast<ImportStmt*>(statement.get())) {
                hasImports_ = true;
                const std::filesystem::path resolvedPath = normalizedPath.parent_path() / importPath(import->path);
                import->resolvedModuleId = loadFile(resolvedPath.string(), true, false, true);
                continue;
            }

            auto* exportStmt = dynamic_cast<ExportStmt*>(statement.get());
            if (!exportStmt || !exportStmt->sourcePath) {
                continue;
            }
            hasImports_ = true;
            const std::filesystem::path resolvedPath = normalizedPath.parent_path() / importPath(*exportStmt->sourcePath);
            exportStmt->resolvedModuleId = loadFile(resolvedPath.string(), true, false, true);
        }

        unit.id = units_.size();
        units_.push_back(std::move(unit));
        canonicalToUnitId_.emplace(canonicalPath, units_.back().id);
        loadingStack_.pop_back();
        return units_.back().id;
    } catch (...) {
        loadingStack_.pop_back();
        throw;
    }
}

Program FrontendSession::assembleProgram()
{
    Program program;
    if (hasImports_) {
        for (ParsedUnit& unit : units_) {
            program.statements.push_back(std::make_unique<ModuleStmt>(
                unit.id,
                unit.path,
                unit.source,
                std::move(unit.statements),
                unit.isEntry));
        }
        return program;
    }

    for (const std::size_t id : entryUnitIds_) {
        ParsedUnit& unit = units_[id];
        for (StmtPtr& statement : unit.statements) {
            program.statements.push_back(std::move(statement));
        }
    }
    return program;
}

void FrontendSession::rebuildCombinedSource()
{
    combinedSource_.clear();
    for (const ParsedUnit& unit : units_) {
        appendWithNewlineSeparation(combinedSource_, unit.source);
    }
}

std::vector<Token> FrontendSession::displayTokens() const
{
    if (!directDisplayTokens_.empty()) {
        return directDisplayTokens_;
    }

    std::vector<Token> display;
    std::string combined;
    for (const ParsedUnit& unit : units_) {
        appendWithNewlineSeparation(combined, "");
        const int lineOffset = lineAtEnd(combined) - 1;
        for (const Token& token : unit.tokens) {
            if (token.type == TokenType::EndOfFile) {
                continue;
            }
            Token shifted = token;
            shifted.line += lineOffset;
            display.push_back(std::move(shifted));
        }
        appendWithNewlineSeparation(combined, unit.source);
    }
    display.push_back(endOfFileToken(combined));
    return display;
}

const std::string& FrontendSession::sourceForDiagnostics() const
{
    return combinedSource_;
}

std::optional<FileDiagnosticError> FrontendSession::remapDirectDiagnostic(const DiagnosticError& error) const
{
    if (!error.location() || directInputs_.size() < 2) {
        return std::nullopt;
    }

    std::size_t startLine = 1;
    for (const DirectInput& input : directInputs_) {
        const std::size_t span = sourceLineSpan(input.source);
        if (span == 0) {
            continue;
        }

        const std::size_t diagnosticLine = static_cast<std::size_t>(error.location()->line);
        if (diagnosticLine >= startLine && diagnosticLine < startLine + span) {
            DiagnosticError remapped(
                error.kind(),
                SourceLocation{
                    static_cast<int>(diagnosticLine - startLine + 1),
                    error.location()->column,
                },
                error.message());
            return FileDiagnosticError(
                remapped,
                DiagnosticSourceContext{input.path, input.source, false});
        }
        startLine += span;
    }

    return std::nullopt;
}

std::size_t FrontendSession::moduleCount() const
{
    return units_.size();
}
