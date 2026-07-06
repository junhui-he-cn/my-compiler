#pragma once

#include <optional>
#include <stdexcept>
#include <string>

enum class DiagnosticKind {
    Lex,
    Parse,
    Type,
    Compile,
    Runtime,
    Import,
};

struct SourceLocation {
    int line = 0;
    int column = 0;
};

class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(DiagnosticKind kind, std::string message);
    DiagnosticError(DiagnosticKind kind, SourceLocation location, std::string message);

    DiagnosticKind kind() const;
    const std::optional<SourceLocation>& location() const;
    const std::string& message() const;

private:
    DiagnosticKind kind_;
    std::optional<SourceLocation> location_;
    std::string message_;
};

std::string diagnosticKindName(DiagnosticKind kind);
std::string formatDiagnostic(
    DiagnosticKind kind,
    const std::optional<SourceLocation>& location,
    const std::string& message);
