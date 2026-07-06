#include "Diagnostic.hpp"

#include <sstream>
#include <utility>

namespace {

std::optional<std::string> sourceLineAt(const std::string& source, int requestedLine)
{
    if (requestedLine <= 0) {
        return std::nullopt;
    }

    int currentLine = 1;
    std::size_t lineStart = 0;
    for (std::size_t index = 0; index <= source.size(); ++index) {
        if (index == source.size() || source[index] == '\n') {
            if (currentLine == requestedLine) {
                return source.substr(lineStart, index - lineStart);
            }
            ++currentLine;
            lineStart = index + 1;
        }
    }

    return std::nullopt;
}

std::size_t caretColumn(int column)
{
    if (column <= 1) {
        return 0;
    }
    return static_cast<std::size_t>(column - 1);
}

} // namespace

std::string diagnosticKindName(DiagnosticKind kind)
{
    switch (kind) {
    case DiagnosticKind::Lex:
        return "Lex";
    case DiagnosticKind::Parse:
        return "Parse";
    case DiagnosticKind::Type:
        return "Type";
    case DiagnosticKind::Compile:
        return "Compile";
    case DiagnosticKind::Runtime:
        return "Runtime";
    case DiagnosticKind::Import:
        return "Import";
    }

    return "Unknown";
}

std::string formatDiagnostic(
    DiagnosticKind kind,
    const std::optional<SourceLocation>& location,
    const std::string& message)
{
    std::string formatted = diagnosticKindName(kind) + " error";
    if (location) {
        formatted += " at " + std::to_string(location->line) + ":" + std::to_string(location->column);
    }
    formatted += ": " + message;
    return formatted;
}

std::string formatDiagnosticWithSource(const DiagnosticError& error, const std::string& source)
{
    std::ostringstream formatted;
    formatted << error.what();

    const std::optional<SourceLocation>& location = error.location();
    if (!location) {
        return formatted.str();
    }

    std::optional<std::string> line = sourceLineAt(source, location->line);
    if (!line) {
        return formatted.str();
    }

    formatted << '\n'
              << "  " << *line << '\n'
              << "  " << std::string(caretColumn(location->column), ' ') << '^';
    return formatted.str();
}

DiagnosticError::DiagnosticError(DiagnosticKind kind, std::string message)
    : std::runtime_error(formatDiagnostic(kind, std::nullopt, message))
    , kind_(kind)
    , message_(std::move(message))
{
}

DiagnosticError::DiagnosticError(DiagnosticKind kind, SourceLocation location, std::string message)
    : std::runtime_error(formatDiagnostic(kind, location, message))
    , kind_(kind)
    , location_(location)
    , message_(std::move(message))
{
}

DiagnosticKind DiagnosticError::kind() const
{
    return kind_;
}

const std::optional<SourceLocation>& DiagnosticError::location() const
{
    return location_;
}

const std::string& DiagnosticError::message() const
{
    return message_;
}
