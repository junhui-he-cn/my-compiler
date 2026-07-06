# Diagnostic Source Snippets Design

## Goal

Implement Phase 15A as a small diagnostic polish slice: CLI diagnostics that already have a line and column should show the relevant combined-source line plus a caret under the reported column.

Example:

```text
Parse error at 1:8: expected expression
  print ;
        ^
```

This keeps the existing first diagnostic line stable while adding context that helps users locate the error faster.

## Scope

Enhance only diagnostics with `SourceLocation`:

- `Lex error at line:column`
- `Parse error at line:column`
- `Type error at line:column`

Diagnostics without a location remain unchanged:

- `Import error: ...`
- `Compile error: ...`
- `Runtime error: ...`
- any other locationless `DiagnosticError`
- non-`DiagnosticError` exceptions such as file open failures

This phase uses the already compiled combined source. It does not remap locations back to original file names or original imported-file line numbers.

## User-Facing Formatting

The first line remains exactly the existing `DiagnosticError::what()` output:

```text
<Kind> error at <line>:<column>: <message>
```

When a source line can be found, append:

```text
  <source line>
  <spaces>^
```

Rules:

- Prefix the source line with two spaces.
- Prefix the caret line with two spaces.
- Add `column - 1` spaces before `^`.
- Treat tabs as one character for now; do not expand tabs visually.
- If the location points at EOF just after the last source character on a line, place the caret after the line text.
- If the source line cannot be found, print only the original one-line diagnostic.
- Do not add snippets to locationless diagnostics.

## Architecture

Add a small formatting helper rather than changing exception construction.

Suggested unit boundary:

```cpp
std::string formatDiagnosticWithSource(const DiagnosticError& error, const std::string& source);
```

Responsibilities:

1. Start from `error.what()` so the stable existing first line is preserved.
2. Inspect `error.location()`.
3. Extract the requested line from the combined source.
4. Append the source line and caret line when extraction succeeds.

`DiagnosticError::what()` and `formatDiagnostic()` should keep returning the current single-line format. This prevents internal code and tests that inspect exception strings from depending on CLI-only context formatting.

`main.cpp` should catch `DiagnosticError` before `std::exception` and print `formatDiagnosticWithSource(error, source)`. The `source` string must be available to the catch block, so declare it outside the `try` and assign it after `SourceManager` loads stdin/files.

## Combined-Source Behavior

This phase intentionally reports snippets from the combined source passed to the lexer/parser/type checker. That means:

- Single-file and stdin diagnostics show the user-written line.
- CLI multi-file diagnostics show the line in the concatenated source.
- Import diagnostics remain locationless and unchanged.
- Diagnostics inside imported source may show the expanded combined-source line, not an original file path.

File-aware diagnostic remapping is future work and should not be implemented in this slice.

## Error Handling

The snippet formatter should be defensive:

- Line numbers are 1-based. Line `0` is invalid and produces no snippet.
- Columns are 1-based. Column `0` should be treated as column `1` for caret placement if a line exists.
- If the requested line is beyond the available source, produce no snippet.
- Empty source lines are valid; show the blank prefixed line and caret at column 1 or the requested column.

## Testing Strategy

Use TDD. Add failing tests before implementation.

Focused coverage:

1. Parse error snippet, for example `print ;`:

```text
Parse error at 1:7: expected expression, found Semicolon `;`
  print ;
        ^
```

2. Type error snippet, for example `print missing;`:

```text
Type error at 1:7: undefined variable `missing`
  print missing;
        ^
```

3. Lex error snippet through a focused CLI test, because lexer errors do not yet have a dedicated golden fixture category.

4. Locationless diagnostics remain one line, at least for import errors. Existing import-error goldens should continue to pass unchanged.

Avoid refreshing all parse/type error goldens in this slice. Update only the fixtures selected for explicit snippet coverage, so the change stays reviewable.

## Documentation

Update:

- `README.md`: diagnostic examples should show source snippets for located diagnostics and mention locationless diagnostics remain one line.
- `AGENTS.md`: update diagnostic convention to include optional source line/caret for located diagnostics.
- `docs/roadmap.md`: mark Phase 15A implemented after completion.

## Out of Scope

- File-aware source maps for imports or multi-file CLI inputs.
- Source filenames in diagnostics.
- Ranges or multi-character underlines.
- Tab expansion/alignment beyond treating each tab as one character.
- Parse recovery or multi-error reporting.
- Changing diagnostic first-line wording.
- Adding a dedicated lexer-error golden fixture category.

## Future Work

A later diagnostics phase can add file-aware source maps to `SourceManager`, preserve original file/line metadata through import expansion, and print diagnostics like:

```text
Type error at lib.cd:3:7: undefined variable `x`
```

That future phase can also add source ranges, multi-line snippets, and parse recovery.
