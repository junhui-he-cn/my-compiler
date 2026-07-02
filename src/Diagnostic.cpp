#include "Diagnostic.hpp"

#include <utility>

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
