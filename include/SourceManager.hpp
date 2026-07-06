#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
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
    std::vector<SourceFile> files_;
};
