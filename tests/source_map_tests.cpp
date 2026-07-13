#include "SourceMap.hpp"

#include <cassert>

int main()
{
    SourceFile file{"demo.cd", "let x = 1;\nprint x;\n"};
    assert(sourceLine(file, SourceLocation{2, 1}) == "print x;");
    assert(sourceLine(file, SourceLocation{9, 1}).empty());
    assert(formatSourceSpan(SourceSpan{0, 2, 7}, {file}) == "demo.cd:2:7");
    return 0;
}
