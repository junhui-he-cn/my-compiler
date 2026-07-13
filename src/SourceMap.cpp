#include "SourceMap.hpp"

#include <sstream>

std::string sourceLine(const SourceFile& file, SourceLocation location)
{
    if (location.line <= 0) {
        return {};
    }

    std::size_t line = 1;
    std::size_t begin = 0;
    for (std::size_t index = 0; index <= file.text.size(); ++index) {
        if (index != file.text.size() && file.text[index] != '\n') {
            continue;
        }

        if (line == static_cast<std::size_t>(location.line)) {
            std::size_t end = index;
            if (end > begin && file.text[end - 1] == '\r') {
                --end;
            }
            return file.text.substr(begin, end - begin);
        }

        ++line;
        begin = index + 1;
    }

    return {};
}

std::string formatSourceSpan(const SourceSpan& span, const std::vector<SourceFile>& files)
{
    const std::string path = span.source < files.size() && !files[span.source].path.empty()
        ? files[span.source].path
        : "<unknown>";
    std::ostringstream output;
    output << path << ':' << span.line << ':' << span.column;
    return output.str();
}
