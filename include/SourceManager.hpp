#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_set>
#include <vector>

struct SourceFile {
    std::string path;
    std::string source;
    std::size_t startLine = 1;
};

class SourceManager {
public:
    std::string loadStdin(std::istream& input);
    std::string loadFiles(const std::vector<std::string>& paths);
    const std::vector<SourceFile>& files() const;

private:
    std::string loadFile(const std::filesystem::path& path, bool isImport);
    std::string expandImports(const std::string& source, const std::filesystem::path& importingFile);
    bool containsTopLevelImportKeyword(const std::string& source) const;

    std::vector<SourceFile> files_;
    std::unordered_set<std::string> loadedFiles_;
    std::vector<std::string> loadingStack_;
};
