#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

struct DiagnosticSourceContext {
    std::string path;
    std::string source;
    bool isStdin = false;
};

class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(DiagnosticKind kind, std::string message);
    DiagnosticError(DiagnosticKind kind, SourceLocation location, std::string message);
    DiagnosticError(DiagnosticKind kind, std::optional<SourceLocation> location, std::string message);

    DiagnosticKind kind() const;
    const std::optional<SourceLocation>& location() const;
    const std::string& message() const;

private:
    DiagnosticKind kind_;
    std::optional<SourceLocation> location_;
    std::string message_;
};

class FileDiagnosticError final : public DiagnosticError {
public:
    FileDiagnosticError(const DiagnosticError& inner, DiagnosticSourceContext context);

    const DiagnosticSourceContext& sourceContext() const;

private:
    DiagnosticSourceContext context_;
};

class FileDiagnosticErrorList final : public std::exception {
public:
    explicit FileDiagnosticErrorList(std::vector<FileDiagnosticError> errors);

    const std::vector<FileDiagnosticError>& errors() const;
    const char* what() const noexcept override;

private:
    std::vector<FileDiagnosticError> errors_;
};

std::string diagnosticKindName(DiagnosticKind kind);
std::string formatDiagnostic(
    DiagnosticKind kind,
    const std::optional<SourceLocation>& location,
    const std::string& message);
std::string formatDiagnosticWithSource(const DiagnosticError& error, const std::string& source);
std::string formatDiagnosticWithSourceContext(const FileDiagnosticError& error);
