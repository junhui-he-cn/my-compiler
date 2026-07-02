# Diagnostic Cleanup Design

## Goal

Implement Phase 5 from `docs/roadmap.md`: make compiler demo errors more consistent and easier to test by introducing a lightweight shared diagnostic foundation and stable output format.

This phase intentionally does not add source snippets, caret rendering, parser recovery, or multi-error reporting.

## Current State

Errors are currently reported by a mix of exception types and ad hoc `std::runtime_error` messages:

- Lexer errors throw `std::runtime_error` with text like `Unexpected character at line 1, column 7: '@'`.
- Parse errors use `ParseError` with text like `Parse error at line 1, column 15: expected expression`.
- Type errors use `TypeError` with text like `Type error: undefined variable `x``.
- IR compile errors use `IRCompileError` with text like `IR compile error: unsupported statement node`.
- IR runtime errors use `IRRuntimeError` with text like `IR runtime error: division by zero`.

The CLI catches `std::exception`, prints `what()` to stderr, and exits with code `1`.

Golden fixtures already separate parse, type, and runtime errors:

- `tests/golden/parse_errors/*.err`
- `tests/golden/type_errors/*.err`
- `tests/golden/runtime_errors/*.run.err`

There is no lexer-error golden category yet.

## Chosen Scope

Use a lightweight shared diagnostic error type and update existing error classes to emit a consistent format.

Do not add source snippets or caret output in this phase.

Do not change exit codes:

- Compiler/language errors still exit `1`.
- CLI usage errors still exit `64`.
- Successful runs still exit `0`.

Do not change successful AST, IR, token, or run output.

## Target Output Format

Diagnostics use one of two forms.

With a source location:

```text
<Kind> error at <line>:<column>: <message>
```

Without a source location:

```text
<Kind> error: <message>
```

Kinds are title-case user-facing labels:

- `Lex`
- `Parse`
- `Type`
- `Compile`
- `Runtime`

Examples:

```text
Lex error at 1:7: unexpected character `@`
Parse error at 1:15: expected expression
Type error at 3:7: undefined variable `x`
Compile error: unsupported statement node
Runtime error: division by zero
```

## Diagnostic Types

Add new files:

```text
include/Diagnostic.hpp
src/Diagnostic.cpp
```

Core declarations:

```cpp
enum class DiagnosticKind {
    Lex,
    Parse,
    Type,
    Compile,
    Runtime,
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
std::string formatDiagnostic(DiagnosticKind kind, const std::optional<SourceLocation>& location, const std::string& message);
```

`DiagnosticError` derives from `std::runtime_error` so `main.cpp` can continue catching `std::exception`. Its `what()` string is the formatted diagnostic.

The class should not be `final`, because existing named error classes should derive from it.

## Existing Error Class Migration

Keep existing class names to reduce churn in callers, but make them derive from `DiagnosticError`.

### ParseError

Change from `std::runtime_error` to `DiagnosticError`:

```cpp
class ParseError final : public DiagnosticError {
public:
    ParseError(const Token& token, const std::string& message);
};
```

Format:

```text
Parse error at 1:15: expected expression
```

### TypeError

Change from `std::runtime_error` to `DiagnosticError` and support both locationless and token-located construction:

```cpp
class TypeError final : public DiagnosticError {
public:
    explicit TypeError(std::string message);
    TypeError(const Token& token, std::string message);
};
```

Located type errors should be used when a relevant token exists.

### IRCompileError

Change from `std::runtime_error` to `DiagnosticError`:

```cpp
class IRCompileError final : public DiagnosticError {
public:
    explicit IRCompileError(std::string message);
};
```

User-facing kind is `Compile`, not `IR compile`.

### IRRuntimeError

Change from `std::runtime_error` to `DiagnosticError`:

```cpp
class IRRuntimeError final : public DiagnosticError {
public:
    explicit IRRuntimeError(std::string message);
};
```

User-facing kind is `Runtime`, not `IR runtime`.

### Lexer Errors

Replace lexer `std::runtime_error` throws for source errors with `DiagnosticError(DiagnosticKind::Lex, SourceLocation{...}, message)`.

Examples:

```text
Lex error at 1:12: unexpected character `|`
Lex error at 2:4: unterminated string
```

Use backticks around offending characters for consistency with existing variable/operator messages.

## Source Location Strategy

### Lexer

Use existing `line_` and `tokenColumn_`.

- Unexpected character location: current token start.
- Unterminated string location: opening quote location.

### Parser

Use the token passed to `ParseError`.

`consume()` should continue to include found-token details when useful:

```text
Parse error at 1:12: expected `{` after while condition, found Print `print`
```

The only format change is `at line 1, column 12` becoming `at 1:12`.

### Type Checker

Use located type errors for user-source mistakes when a token is available.

Map locations as follows:

- Duplicate declaration: duplicate declaration name token.
- Undefined variable read: variable name token.
- Undefined assignment target: assignment name token.
- Unknown type annotation: annotation token.
- Let initializer mismatch: declaration name token.
- Assignment type mismatch: assignment name token.
- Unary operand mismatch: unary operator token.
- Binary operand mismatch: binary operator token.
- Unsupported unary/binary operator defensive errors: operator token if available.

Keep internal defensive errors locationless:

- scope stack is empty
- unsupported statement node
- unsupported expression node

### Compile and Runtime

IR compile and runtime errors remain locationless in this phase because there is no source-map infrastructure between IR instructions and AST tokens.

## Golden Fixture Updates

Update all existing error expected files:

- `tests/golden/parse_errors/*.err`
- `tests/golden/type_errors/*.err`
- `tests/golden/runtime_errors/*.run.err`

Expected broad changes:

- `Parse error at line 1, column 15: ...` becomes `Parse error at 1:15: ...`.
- `Type error: ...` becomes `Type error at line:column: ...` when a relevant token exists.
- `IR runtime error: ...` becomes `Runtime error: ...`.
- `IR compile error: ...` becomes `Compile error: ...` if any golden fixture covers compile errors.

Do not add a `lex_errors` fixture category in this phase. The runner has parse, type, and runtime error categories only. Lexer diagnostics are improved in implementation, but no new golden category is added.

## CLI Behavior

`src/main.cpp` may continue to catch `std::exception` and print `error.what()`.

Option parsing and usage errors are not diagnostics in this phase:

- `--help` still exits `0`.
- invalid CLI usage still prints usage and exits `64`.
- failed input-file open may remain a plain runtime exception unless implementation chooses to wrap it as a diagnostic. This is outside the language diagnostic scope and should not require golden changes.

## Documentation Updates

Update `README.md` with a short error-output note:

- The compiler reports `Lex`, `Parse`, `Type`, `Compile`, and `Runtime` errors.
- Front-end diagnostics include `line:column` where available.
- Runtime diagnostics currently do not include source locations.

Update `docs/roadmap.md`:

- Mark Phase 5 as implemented after implementation is complete.
- Note that snippets/carets and multi-error recovery are still future improvements.

Update `AGENTS.md`:

- Document the diagnostic output convention.
- Remind agents to refresh parse/type/runtime error goldens after intentional diagnostic format changes.
- Note that lexer errors do not yet have a golden fixture category.

## Testing Strategy

Use golden tests as the behavior contract.

Recommended red-green sequence:

1. Add diagnostic infrastructure and update one parse-error fixture first.
2. Verify that fixture fails before format changes or immediately after adding the expected new output.
3. Update parser formatting and verify parse-error goldens.
4. Update type-error expected files and type-checker locations.
5. Update runtime-error expected files and IR runtime formatting.
6. Run full verification.

Selftests for the golden runner should not need changes because fixture discovery and comparison rules remain unchanged.

## Non-Goals

- Do not print source snippets.
- Do not print caret indicators.
- Do not collect multiple errors.
- Do not implement parser recovery.
- Do not add warning diagnostics.
- Do not change success output.
- Do not change error exit code `1`.
- Do not change CLI usage exit code `64`.
- Do not add a `lex_errors` golden category.
- Do not map IR runtime errors back to source locations.
