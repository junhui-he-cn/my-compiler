# Import Source Loading Design

## Goal

Implement Phase 14B as a minimal source-loading import feature:

```cd
import "./lib.cd";
print add(1, 2);
```

The import system should build on the existing `SourceManager` multi-file foundation. It should let programs reference secondary source files from within source code, without introducing module namespaces, `export`, private visibility, or separate compilation.

## User-Facing Semantics

### Syntax

Add a top-level declaration form:

```ebnf
declaration = importDecl | structDecl | funDecl | letDecl | statement ;
importDecl  = "import", string, ";" ;
```

Only string-literal paths are accepted:

```cd
import "./math.cd";
```

These are invalid:

```cd
import path;
import "./math.cd"
{
  import "./nested.cd";
}
```

### Visibility and Program Model

Imports are source-loading directives. They do not create runtime values and do not appear in the final AST print.

Imported files are expanded at the import declaration's position. All top-level declarations from the main file and imported files share the same program scope after expansion.

Example:

```cd
// math.cd
fun add(a, b) { return a + b; }

// main.cd
import "./math.cd";
print add(1, 2);
```

is equivalent to:

```cd
fun add(a, b) { return a + b; }
print add(1, 2);
```

There is no namespace and no `export` keyword in this phase. Existing duplicate-declaration rules continue to apply after expansion.

### Ordering

Import expansion preserves source order. Declarations in an imported file appear exactly where the import declaration was written.

```cd
print value;       // type error if value is not already defined
import "./lib.cd";
```

This behaves the same as writing `lib.cd` after the first print.

### Duplicate Imports

The same canonical file path should be loaded at most once per source graph. A second import of an already-loaded file expands to nothing.

This rule prevents accidental duplicate top-level declarations for shared utility modules:

```cd
import "./math.cd";
import "./math.cd"; // no-op
```

### CLI Multi-File Interaction

Phase 14A CLI multi-file loading remains supported. Each CLI input file is treated as an entry source. The combined program order is the command-line order, with each entry source recursively expanding its imports.

If two CLI entry files import the same dependency, the dependency is included only once globally.

## Path Resolution

Import paths are resolved relative to the importing file's directory.

- `main.cd` importing `"./lib.cd"` resolves relative to `main.cd`.
- `lib/util.cd` importing `"./nested.cd"` resolves relative to `lib/`.
- CLI input paths may be relative to the current working directory, matching existing file-open behavior.

Internally, resolved import paths should be canonicalized where possible so duplicate and cycle detection are stable across `./lib.cd` and `lib.cd` spellings.

For this phase, imports from stdin are not supported. If source is read from stdin and contains an import declaration, loading should fail with:

```text
Import error: import is not supported from stdin
```

This avoids ambiguous relative-path roots. A future phase can add `--source-root` or similar if needed.

## Error Handling

Add a distinct `Import error:` diagnostic category by throwing a `DiagnosticError` with kind `Import`, or by adding a small `ImportError` type that formats with that prefix. Use locationless diagnostics for this phase.

Required errors:

- Missing import file:

```text
Import error: failed to open import: <path>
```

- Import from stdin:

```text
Import error: import is not supported from stdin
```

- Import cycle:

```text
Import error: import cycle detected: a.cd -> b.cd -> a.cd
```

The cycle path should use normalized paths when available. Exact absolute vs relative spelling can be chosen during implementation and then fixed in goldens.

Syntax errors remain parse errors. For example, `import path;` should be parsed as an import declaration and then fail because the path token is not a string, with a location-bearing parse error such as:

```text
Parse error at 1:8: expected import path string
```

## Architecture

### Lexer and Parser

Add `import` as a keyword token.

The parser should recognize `import` only in `declaration()` at top level or inside blocks where declarations are currently accepted. However, the language-level rule is that imports are source-loading directives only. The simplest robust implementation is to reject imports after raw source loading rather than generate AST nodes. Two approaches are possible:

1. Pre-parse imports in `SourceManager` and remove/expand them before normal lexing.
2. Add `ImportDeclStmt` to the AST and have a loader parse enough structure to resolve it.

Use approach 1 for this phase: source loading scans for `import "path";` directives, expands them to imported source text, and replaces the directive with whitespace/newlines so normal lexer/parser never sees `import` in valid programs.

Because approach 1 is intentionally limited, it should only recognize imports at line/statement positions outside strings and comments. It must not expand text that appears inside string literals or comments.

Even with source-level expansion, adding `import` as a reserved keyword is still valuable so stray imports that reach the parser produce clearer errors and future AST support has a token ready.

### SourceManager

Extend `SourceManager` to:

- Load entry files with import expansion.
- Track a `loadedFiles` set of canonical paths for duplicate suppression.
- Track a loading stack for cycle detection.
- Resolve imports relative to the current file's parent directory.
- Reject imports in stdin-loaded source.
- Preserve newline separation around expanded imports.

Suggested internal model:

```cpp
struct LoadedSource {
    std::string displayPath;
    std::string canonicalPath;
    std::string source;
};
```

`loadFiles(paths)` should produce one combined source after recursive expansion. `loadStdin(input)` should either return input unchanged when there are no imports, or throw the stdin import error when it finds an import directive.

### Import Scanner

The import scanner should be deliberately small and conservative:

- Skip whitespace.
- Skip `//` comments.
- Skip string literals with existing escape behavior.
- When it sees the identifier `import` outside strings/comments, parse:
  - optional whitespace
  - a double-quoted path literal
  - optional whitespace
  - `;`
- Expand that directive by recursively loading the referenced file.
- If malformed, leave it to the normal lexer/parser when possible, or throw an import/parse-style error if the loader cannot continue safely.

This is not intended to be a full parser. The full parser still handles language syntax after import expansion.

## Testing Strategy

### Success Fixtures

Add golden fixtures for:

1. Basic import:

```cd
// lib.cd
fun add(a, b) { return a + b; }

// input.cd
import "./lib.cd";
print add(1, 2);
```

Expected run output:

```text
3
```

The AST should show only the function and print, not the import declaration.

2. Nested import:

```cd
// inner.cd
let value = 41;

// lib.cd
import "./inner.cd";
fun next() { return value + 1; }

// input.cd
import "./lib.cd";
print next();
```

3. Duplicate import no-op:

```cd
import "./lib.cd";
import "./lib.cd";
print value;
```

where `lib.cd` defines `let value = 1;`. This should not trigger duplicate declaration errors.

At least one import fixture should include `ir.out`, `bytecode.out`, and Rust VM parity coverage.

### Error Fixtures

Add import-error fixtures for:

- Missing imported file.
- Import cycle.
- Import from stdin, covered by a focused CLI integration test because golden fixtures use files.

Add parse-error fixtures for malformed import syntax:

- `import path;`
- `import "./lib.cd"` without semicolon.

### Test Runner Considerations

Existing golden fixtures can use normal `input.cd` and sidecar files. No new runner format is required for basic import fixtures because the source file itself names dependencies.

Bytecode artifact and Rust VM tests should compile the importing `input.cd`; the compiler performs import expansion before bytecode emission.

## Documentation

Update:

- `README.md`: document `import "path";`, relative path behavior, duplicate import behavior, and no namespace/export yet.
- `docs/language-grammar.ebnf`: add `importDecl` to declarations.
- `docs/roadmap.md`: mark Phase 14B implemented after completion.
- `AGENTS.md`: document current import semantics and limitations.

## Out of Scope

- `import name from "path"`.
- namespaces or module objects.
- `export` declarations.
- private module visibility.
- file-aware diagnostic remapping.
- package search paths.
- imports from stdin.
- incremental compilation.

## Future Work

A later Phase 14C can replace source-level import expansion with AST-level module loading if the language adds namespaces, exports, or file-aware diagnostics. The current design intentionally favors a small, working source-loading feature over a complete module system.
