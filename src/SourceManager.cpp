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
    loadedFiles_.clear();
    loadingStack_.clear();

    std::string source = readAll(input);
    if (containsTopLevelImportKeyword(source)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }
    return source;
}

std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    files_.clear();
    loadedFiles_.clear();
    loadingStack_.clear();

    std::string combined;
    for (const std::string& path : paths) {
        if (!combined.empty() && combined.back() != '\n') {
            combined.push_back('\n');
        }
        combined += loadFile(path, false);
    }
    return combined;
}

const std::vector<SourceFile>& SourceManager::files() const
{
    return files_;
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
