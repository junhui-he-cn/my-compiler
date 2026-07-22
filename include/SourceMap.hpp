#pragma once

#include "Diagnostic.hpp"
#include "SourceIdentity.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct SourceFile {
    std::string path;
    std::string text;
    SourceFileId id{};
};

struct SourceSpan {
    std::size_t source = 0;
    int line = 0;
    int column = 0;
};

std::string sourceLine(const SourceFile& file, SourceLocation location);
SourcePosition sourcePositionAt(const SourceFile& file, std::size_t byte);
SourcePosition sourcePositionAt(const std::vector<SourceFile>& files, const SourceRange& range);
bool isValidSourceRange(const SourceRange& range, const std::vector<SourceFile>& files);
std::string formatSourceSpan(const SourceSpan& span, const std::vector<SourceFile>& files);
std::string formatSourceRange(const SourceRange& range, const std::vector<SourceFile>& files);
