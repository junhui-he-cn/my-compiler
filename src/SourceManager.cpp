#include "SourceManager.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

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

} // namespace

std::string SourceManager::loadStdin(std::istream& input)
{
    files_.clear();
    return readAll(input);
}

std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    files_.clear();
    std::string combined;
    std::size_t nextStartLine = 1;

    for (const std::string& path : paths) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("failed to open input file: " + path);
        }

        std::string source = readAll(file);
        if (!combined.empty() && combined.back() != '\n') {
            combined.push_back('\n');
            ++nextStartLine;
        }

        files_.push_back(SourceFile{path, source, nextStartLine});
        combined += source;
        nextStartLine += lineCount(source) - 1;
    }

    return combined;
}

const std::vector<SourceFile>& SourceManager::files() const
{
    return files_;
}
