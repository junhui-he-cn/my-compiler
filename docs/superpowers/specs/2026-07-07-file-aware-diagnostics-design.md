# File-Aware Diagnostics Design

Date: 2026-07-07

## Goal

Implement Phase 15C: file-aware front-end diagnostics for multi-file and imported programs.

When lexing, parsing, or type checking reports a located diagnostic from a file-backed source unit, the CLI should print the original file path, the line and column within that file, and the source snippet from that file. This replaces the current combined-source-only diagnostic display for multi-file/imported programs.

## User-visible behavior

For file-backed source units, located front-end diagnostics should use:

```text
<Kind> error at <path>:<line>:<column>: <message>
  <source line>
  <caret>
```

Example:

```text
Type error at tests/golden/app/lib.cd:2:7: undefined variable `x`
  print x;
        ^
```

For stdin, diagnostics remain unchanged:

```text
Parse error at 1:7: expected expression
  print ;
        ^
```

This slice applies to:

- Lexer errors.
- Parse errors.
- Type errors.

This slice keeps unchanged:

- Locationless compile errors.
- Locationless runtime errors.
- Locationless import loading errors.
- Rust VM runtime diagnostics.

## Scope and non-goals

This slice does not add:

- Runtime stack traces.
- Located import loading errors.
- LSP/JSON structured diagnostics.
- A new source-location model in every token.
- File-aware Rust VM errors.
- Path-shortening or cwd-relative formatting policy beyond using the path already recorded by `SourceManager`.

## Architecture

Use a diagnostic source-context wrapper rather than changing `Token` or `SourceLocation`.

Add a small context structure such as:

```cpp
struct DiagnosticSourceContext {
    std::string path;
    std::string source;
    bool isStdin = false;
};
```

Add an exception wrapper, for example:

```cpp
class FileDiagnosticError final : public DiagnosticError {
public:
    FileDiagnosticError(const DiagnosticError& inner, DiagnosticSourceContext context);
    const DiagnosticSourceContext& sourceContext() const;
};
```

The wrapper preserves:

- Diagnostic kind.
- Local source location.
- Original message.
- Source path/source text context.

`main.cpp` can catch `FileDiagnosticError` before `DiagnosticError` and format with the context. Existing `DiagnosticError` handling remains available for stdin and locationless errors.

## Data flow

### Stdin

Stdin keeps the current single-source path:

1. `SourceManager::loadStdin()` returns a source string.
2. Lexer/parser/type checker may throw `DiagnosticError` with local line/column.
3. `main.cpp` formats with `formatDiagnosticWithSource(error, source)`.
4. Output remains pathless.

### File inputs without imports

Direct CLI files without import may continue through the combined-source path for compatibility, except when a located diagnostic can be associated with a file-backed unit. The implementation plan should choose the smallest safe route:

- Either parse file units individually and wrap diagnostics with each unit's path/source.
- Or preserve combined parsing but use a SourceManager line map to remap combined locations back to file units.

The user-visible requirement is that direct multi-file diagnostics identify the original file.

### Module/import programs

For imported/module-aware programs:

1. `SourceManager::loadFileUnits()` returns `SourceUnit` records containing path and source.
2. `buildModuleProgram()` lexes/parses each unit independently.
3. If lexing/parsing a unit throws a located `DiagnosticError`, wrap it with that unit's path/source.
4. `ModuleStmt` keeps path/source or enough module metadata for later type checking.
5. `TypeChecker::checkModule()` wraps located type errors thrown while checking a module with the module's path/source.
6. `main.cpp` formats the wrapped diagnostic with file-aware output.

## Formatting

Add a formatter such as:

```cpp
std::string formatDiagnosticWithSourceContext(const FileDiagnosticError& error);
```

Rules:

- If the wrapped diagnostic has no location, fall back to `error.what()`.
- If context is stdin or path is empty, use current pathless format.
- If context is a file path, print `Kind error at path:line:column: message`.
- Snippet lookup uses context source and local line number.
- Caret placement uses local column number and the existing caret-column behavior.

Avoid changing `DiagnosticError::what()` for ordinary diagnostics; this keeps existing first-line compatibility where tests expect it.

## Error boundaries

Wrap only located front-end diagnostics:

- Lex/parse errors from per-unit parsing.
- Type errors thrown while checking a module or file unit.

Do not wrap:

- Import errors from source loading.
- Runtime errors.
- Compile errors from IR/bytecode stages unless they already have a trustworthy file context in a future slice.

## Testing strategy

Add focused CLI tests in `tests/cli_multi_source_tests.py` for:

1. Imported file parse error shows imported file path, file-local line/column, line text, and caret.
2. Imported file type error shows imported file path, file-local line/column, line text, and caret.
3. Direct multi-file parse error shows the file containing the parse error.
4. Direct multi-file type error shows the file containing the type error.
5. Stdin located diagnostics keep the current pathless format.
6. Import loading errors stay one-line and pathless unless already locationless formatted.

Add a small number of golden fixtures with full multiline `.err` files only where caret/file behavior is intentional. Existing parse/type `.err` files may remain one-line because the golden runner already accepts actual snippets for located diagnostics when expected output is one-line.

Update tests if the first line changes from:

```text
Type error at 2:7: ...
```

to:

```text
Type error at path.cd:2:7: ...
```

for file-backed exact fixtures.

## Documentation updates

Update:

- `README.md` diagnostic section: file-backed diagnostics include file paths; stdin remains pathless.
- `docs/roadmap.md`: mark Phase 15C implemented after implementation.
- `AGENTS.md`: update diagnostic convention and remove the limitation that diagnostics report combined-source locations for file inputs.

## Open implementation choices

The implementation plan should decide:

- Whether direct multi-file non-import programs should be parsed as file units or remapped through a line map.
- Whether `ModuleStmt` stores full source text or a module id that resolves through a side table.
- Exact class names for file-aware diagnostic wrappers.

These choices must preserve the user-visible behavior described above.
