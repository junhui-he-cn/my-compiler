#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SourceFile {
    std::string path;
    std::string source;
    std::size_t startLine = 1;
};

struct SourceImport {
    std::string requestedPath;
    std::string canonicalPath;
};

struct SourceUnit {
    std::size_t id = 0;
    std::string path;
    std::string canonicalPath;
    std::string source;
    bool isEntry = false;
    std::vector<SourceImport> imports;
};

struct SourceLoadResult {
    std::string combinedSource;
    std::vector<SourceUnit> units;
    std::vector<std::size_t> entryUnitIds;
};

class SourceManager {
public:
    std::string loadStdin(std::istream& input);
    std::string loadFiles(const std::vector<std::string>& paths);
    SourceLoadResult loadStdinUnit(std::istream& input);
    SourceLoadResult loadFileUnits(const std::vector<std::string>& paths);
    const std::vector<SourceFile>& files() const;

private:
    std::string loadFile(const std::filesystem::path& path, bool isImport);
    std::string expandImports(const std::string& source, const std::filesystem::path& importingFile);
    bool containsTopLevelImportKeyword(const std::string& source) const;
    std::size_t loadFileUnit(const std::filesystem::path& path, bool isImport, bool isEntry);
    std::vector<SourceImport> scanImportDirectives(const std::string& source, const std::filesystem::path& importingFile);

    std::vector<SourceFile> files_;
    std::vector<SourceUnit> units_;
    std::unordered_set<std::string> loadedFiles_;
    std::unordered_map<std::string, std::size_t> canonicalToUnitId_;
    std::vector<std::string> loadingStack_;
};
