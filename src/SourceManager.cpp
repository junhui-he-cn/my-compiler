#include "SourceManager.hpp"

#include "Diagnostic.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace {

std::string readAll(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::size_t lineCount(const std::string& source)
{
    std::size_t lines = 1;
    for (char ch : source) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
}

bool isIdentifierStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierPart(char ch)
{
    return isIdentifierStart(ch) || std::isdigit(static_cast<unsigned char>(ch));
}

bool isBoundaryBefore(const std::string& source, std::size_t index)
{
    return index == 0 || !isIdentifierPart(source[index - 1]);
}

bool isBoundaryAfter(const std::string& source, std::size_t index)
{
    return index >= source.size() || !isIdentifierPart(source[index]);
}

bool startsWithImportKeyword(const std::string& source, std::size_t index)
{
    constexpr const char* keyword = "import";
    constexpr std::size_t keywordLength = 6;
    return index + keywordLength <= source.size()
        && source.compare(index, keywordLength, keyword) == 0
        && isBoundaryBefore(source, index)
        && isBoundaryAfter(source, index + keywordLength);
}

void skipWhitespace(const std::string& source, std::size_t& index)
{
    while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index]))) {
        ++index;
    }
}

bool parseImportDirective(
    const std::string& source,
    std::size_t start,
    std::string& path,
    std::size_t& afterDirective)
{
    std::size_t index = start + 6;
    skipWhitespace(source, index);
    if (index >= source.size() || source[index] != '"') {
        return false;
    }

    const std::size_t pathStart = index + 1;
    ++index;
    while (index < source.size() && source[index] != '"') {
        ++index;
    }
    if (index >= source.size()) {
        return false;
    }

    path = source.substr(pathStart, index - pathStart);
    ++index;
    skipWhitespace(source, index);

    if (index + 2 <= source.size()
        && source.compare(index, 2, "as") == 0
        && isBoundaryBefore(source, index)
        && isBoundaryAfter(source, index + 2)) {
        index += 2;
        skipWhitespace(source, index);
        if (index >= source.size() || !isIdentifierStart(source[index])) {
            return false;
        }
        ++index;
        while (index < source.size() && isIdentifierPart(source[index])) {
            ++index;
        }
        skipWhitespace(source, index);
    }

    if (index >= source.size() || source[index] != ';') {
        return false;
    }

    afterDirective = index + 1;
    return true;
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
    auto found = std::find(stack.begin(), stack.end(), repeated);
    std::ostringstream out;
    if (found == stack.end()) {
        out << repeated << " -> " << repeated;
        return out.str();
    }

    for (auto it = found; it != stack.end(); ++it) {
        if (it != found) {
            out << " -> ";
        }
        out << *it;
    }
    out << " -> " << repeated;
    return out.str();
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

} // namespace

std::string SourceManager::loadStdin(std::istream& input)
{
    files_.clear();
    units_.clear();
    loadedFiles_.clear();
    canonicalToUnitId_.clear();
    loadingStack_.clear();

    std::string source = readAll(input);
    if (containsTopLevelImportKeyword(source)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }
    return source;
}

std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    return loadFileUnits(paths).combinedSource;
}

const std::vector<SourceFile>& SourceManager::files() const
{
    return files_;
}

SourceLoadResult SourceManager::loadStdinUnit(std::istream& input)
{
    files_.clear();
    units_.clear();
    loadedFiles_.clear();
    canonicalToUnitId_.clear();
    loadingStack_.clear();

    std::string source = readAll(input);
    if (containsTopLevelImportKeyword(source)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }

    units_.push_back(SourceUnit{0, "<stdin>", "<stdin>", source, true, {}});
    return SourceLoadResult{source, units_, {0}};
}

SourceLoadResult SourceManager::loadFileUnits(const std::vector<std::string>& paths)
{
    files_.clear();
    units_.clear();
    loadedFiles_.clear();
    canonicalToUnitId_.clear();
    loadingStack_.clear();

    std::vector<std::size_t> entryUnitIds;
    for (const std::string& path : paths) {
        entryUnitIds.push_back(loadFileUnit(path, false, true));
    }

    std::string combined;
    for (const SourceUnit& unit : units_) {
        appendWithNewlineSeparation(combined, unit.source);
    }
    return SourceLoadResult{combined, units_, std::move(entryUnitIds)};
}

std::size_t SourceManager::loadFileUnit(const std::filesystem::path& path, bool isImport, bool isEntry)
{
    const std::filesystem::path normalizedPath = normalizedExistingPath(path);
    const std::string canonical = pathString(normalizedPath);
    const std::string display = pathString(path);

    if (std::find(loadingStack_.begin(), loadingStack_.end(), canonical) != loadingStack_.end()) {
        throw DiagnosticError(DiagnosticKind::Import, "import cycle detected: " + displayCycle(loadingStack_, canonical));
    }

    const auto loaded = canonicalToUnitId_.find(canonical);
    if (loaded != canonicalToUnitId_.end()) {
        if (isEntry) {
            units_[loaded->second].isEntry = true;
        }
        return loaded->second;
    }

    std::ifstream file(path);
    if (!file) {
        if (isImport) {
            throw DiagnosticError(DiagnosticKind::Import, "failed to open import: " + display);
        }
        throw std::runtime_error("failed to open input file: " + display);
    }

    loadingStack_.push_back(canonical);
    std::string source = readAll(file);
    std::vector<SourceImport> imports = scanImportDirectives(source, normalizedPath);
    for (const SourceImport& sourceImport : imports) {
        loadFileUnit(std::filesystem::path(sourceImport.canonicalPath), true, false);
    }
    loadingStack_.pop_back();

    const std::size_t id = units_.size();
    units_.push_back(SourceUnit{id, display, canonical, std::move(source), isEntry, std::move(imports)});
    canonicalToUnitId_.emplace(canonical, id);
    loadedFiles_.insert(canonical);
    return id;
}

std::vector<SourceImport> SourceManager::scanImportDirectives(
    const std::string& source,
    const std::filesystem::path& importingFile)
{
    std::vector<SourceImport> imports;
    int braceDepth = 0;

    for (std::size_t index = 0; index < source.size();) {
        const char ch = source[index];

        if (ch == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            while (index < source.size() && source[index] != '\n') {
                ++index;
            }
            continue;
        }

        if (ch == '"') {
            ++index;
            while (index < source.size()) {
                if (source[index++] == '"') {
                    break;
                }
            }
            continue;
        }

        if (ch == '{') {
            ++braceDepth;
            ++index;
            continue;
        }
        if (ch == '}') {
            if (braceDepth > 0) {
                --braceDepth;
            }
            ++index;
            continue;
        }

        if (braceDepth == 0 && startsWithImportKeyword(source, index)) {
            std::string importPath;
            std::size_t afterDirective = index;
            if (parseImportDirective(source, index, importPath, afterDirective)) {
                const std::filesystem::path resolved = importingFile.parent_path() / importPath;
                const std::filesystem::path canonicalPath = normalizedExistingPath(resolved);
                imports.push_back(SourceImport{importPath, pathString(canonicalPath)});
                index = afterDirective;
                continue;
            }
        }

        ++index;
    }

    return imports;
}

std::string SourceManager::loadFile(const std::filesystem::path& path, bool isImport)
{
    const std::filesystem::path normalizedPath = normalizedExistingPath(path);
    const std::string canonical = pathString(normalizedPath);
    const std::string display = pathString(path);

    if (std::find(loadingStack_.begin(), loadingStack_.end(), canonical) != loadingStack_.end()) {
        throw DiagnosticError(DiagnosticKind::Import, "import cycle detected: " + displayCycle(loadingStack_, canonical));
    }
    if (loadedFiles_.find(canonical) != loadedFiles_.end()) {
        return "";
    }

    std::ifstream file(path);
    if (!file) {
        if (isImport) {
            throw DiagnosticError(DiagnosticKind::Import, "failed to open import: " + display);
        }
        throw std::runtime_error("failed to open input file: " + display);
    }

    loadingStack_.push_back(canonical);
    std::string source = readAll(file);
    std::string expanded = expandImports(source, normalizedPath);
    loadingStack_.pop_back();

    loadedFiles_.insert(canonical);

    std::size_t startLine = 1;
    if (!files_.empty()) {
        const SourceFile& previous = files_.back();
        startLine = previous.startLine + lineCount(previous.source) - 1;
        if (!previous.source.empty() && previous.source.back() != '\n') {
            ++startLine;
        }
    }
    files_.push_back(SourceFile{display, expanded, startLine});

    return expanded;
}

std::string SourceManager::expandImports(const std::string& source, const std::filesystem::path& importingFile)
{
    std::string output;
    output.reserve(source.size());
    int braceDepth = 0;

    for (std::size_t index = 0; index < source.size();) {
        const char ch = source[index];

        if (ch == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            while (index < source.size() && source[index] != '\n') {
                output.push_back(source[index++]);
            }
            continue;
        }

        if (ch == '"') {
            output.push_back(source[index++]);
            while (index < source.size()) {
                const char stringChar = source[index];
                output.push_back(stringChar);
                ++index;
                if (stringChar == '"') {
                    break;
                }
            }
            continue;
        }

        if (ch == '{') {
            ++braceDepth;
            output.push_back(ch);
            ++index;
            continue;
        }
        if (ch == '}') {
            if (braceDepth > 0) {
                --braceDepth;
            }
            output.push_back(ch);
            ++index;
            continue;
        }

        if (braceDepth == 0 && startsWithImportKeyword(source, index)) {
            std::string importPath;
            std::size_t afterDirective = index;
            if (parseImportDirective(source, index, importPath, afterDirective)) {
                const std::filesystem::path resolved = importingFile.parent_path() / importPath;
                appendWithNewlineSeparation(output, loadFile(resolved, true));
                index = afterDirective;
                continue;
            }
        }

        output.push_back(ch);
        ++index;
    }

    return output;
}

bool SourceManager::containsTopLevelImportKeyword(const std::string& source) const
{
    int braceDepth = 0;
    for (std::size_t index = 0; index < source.size();) {
        const char ch = source[index];

        if (ch == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            while (index < source.size() && source[index] != '\n') {
                ++index;
            }
            continue;
        }

        if (ch == '"') {
            ++index;
            while (index < source.size()) {
                const char stringChar = source[index++];
                if (stringChar == '"') {
                    break;
                }
            }
            continue;
        }

        if (ch == '{') {
            ++braceDepth;
            ++index;
            continue;
        }
        if (ch == '}') {
            if (braceDepth > 0) {
                --braceDepth;
            }
            ++index;
            continue;
        }

        if (braceDepth == 0 && startsWithImportKeyword(source, index)) {
            return true;
        }
        ++index;
    }
    return false;
}
