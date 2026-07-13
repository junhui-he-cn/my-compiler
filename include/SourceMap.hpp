#pragma once

#include "Diagnostic.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct SourceFile {
    std::string path;
    std::string text;
};

struct SourceSpan {
    std::size_t source = 0;
    int line = 0;
    int column = 0;
};

std::string sourceLine(const SourceFile& file, SourceLocation location);
std::string formatSourceSpan(const SourceSpan& span, const std::vector<SourceFile>& files);
