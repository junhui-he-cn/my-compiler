# CLI Multi-Source Compilation Design

## Goal

Implement Phase 14A as a source-management foundation without adding `import` syntax yet. The compiler CLI should accept multiple input files, concatenate them in command-line order, and compile the combined source as one program.

This gives immediate user value for splitting programs across files while keeping the language semantics simple. It also creates a focused place to evolve future `import` loading and file-aware diagnostics.

## User-Facing Behavior

The CLI accepts zero or more input paths for normal modes:

```sh
compiler_design lib.cd main.cd
compiler_design --run lib.cd main.cd
compiler_design --ir lib.cd main.cd
compiler_design --bytecode lib.cd main.cd
compiler_design --emit-bytecode out.cdbc lib.cd main.cd
```

If no input path is provided, existing stdin behavior remains unchanged:

```sh
printf 'print 1;' | compiler_design --run
```

When multiple files are provided, the compiler reads them in command-line order and compiles them as if their contents were written in one source file with a newline inserted between files. Example:

```cd
// lib.cd
fun add(a, b) { return a + b; }

// main.cd
print add(1, 2);
```

```sh
compiler_design --run lib.cd main.cd
```

prints:

```text
3
```

Top-level declarations all share one program scope after concatenation. This means ordering matters exactly as it would in one file. If the current language requires a declaration to be visible before a use in a given context, splitting files does not change that rule.

## Command-Line Parsing

Current CLI parsing stores one `inputPath`. Replace that with `std::vector<std::string> inputPaths`.

Rules:

- Any non-option argument is an input path.
- Multiple input paths are accepted in all normal output modes.
- `--emit-bytecode output.cdbc` still consumes its output path first, then accepts one or more input paths.
- `--emit-bytecode` with no input paths is allowed and reads from stdin only if that behavior is explicitly simpler to preserve; otherwise it may keep requiring at least one file. For this phase, preserve the current requirement that `--emit-bytecode` needs input paths so artifact generation has a deterministic source root for future imports.
- Unknown options or invalid option combinations still print usage and exit 64.

Update usage to show plural files:

```text
Usage: compiler_design [--tokens] [--ir] [--bytecode] [--run] [file ...]
       compiler_design --emit-bytecode output.cdbc file [...]
If no file is provided, source is read from stdin except for --emit-bytecode, which requires at least one file.
```

## SourceManager

Add a small `SourceManager` unit rather than keeping file concatenation in `main.cpp`.

Responsibilities:

- Read all requested source files.
- Preserve current stdin reading when no input path is provided.
- Concatenate sources in order.
- Insert exactly one `\n` between files when combining, including when a previous file does not end with a newline.
- Store lightweight source metadata for future import/file-aware diagnostics.

Suggested interface:

```cpp
struct SourceFile {
    std::string path;
    std::string source;
    std::size_t startLine = 1;
};

class SourceManager {
public:
    std::string loadStdin(std::istream& input);
    std::string loadFiles(const std::vector<std::string>& paths);
    const std::vector<SourceFile>& files() const;
};
```

`loadFiles` should throw `std::runtime_error("failed to open input file: " + path)` for missing files, matching the existing error wording. `loadStdin` returns the stdin contents and clears file metadata or records no files.

The lexer still receives one combined source string. No token location changes are required in this phase.

## Diagnostics

This phase intentionally keeps diagnostics line/column based on the combined source. It does not add file names to lex, parse, type, compile, or runtime errors.

This is an explicit limitation. The `SourceManager` metadata exists so a later phase can map combined-source line numbers back to original files without changing the CLI again.

## Testing Strategy

### Golden Success Fixtures

Extend the golden runner to support success fixtures with additional source files. A fixture may include an `args.txt` file whose whitespace-separated entries are appended after mode flags instead of the default single `input.cd` path.

Example fixture layout:

```text
tests/golden/multi_file_functions/
  lib.cd
  main.cd
  args.txt
  run.out
  ast.out
```

`args.txt`:

```text
lib.cd main.cd
```

The runner should resolve entries relative to the fixture directory. Existing fixtures without `args.txt` keep using `input.cd` exactly as they do today.

Add success fixtures for:

- multi-file function call: `lib.cd` defines `add`, `main.cd` prints it.
- ordering behavior: files are concatenated in the order listed in `args.txt`.
- newline separation: first file without trailing newline followed by second file still parses as separate declarations.

### CLI Error Fixture or Selftest

Add a focused test for missing file behavior. This can be a golden runner selftest or a direct golden fixture if the runner supports it cleanly. Expected stderr:

```text
failed to open input file: <path>
```

### Bytecode and Rust VM Parity

At least one multi-file fixture should include `bytecode.out` or a bytecode artifact case. The Rust VM path should be covered through an artifact fixture or by adding a golden allowlist case once the compiler can emit bytecode from multiple inputs.

## Documentation

Update:

- `README.md`: document multiple input file CLI behavior and stdin fallback.
- `docs/roadmap.md`: mark Phase 14A source-management/multi-file CLI as implemented once complete, while `import` syntax remains future work.
- `AGENTS.md`: document that CLI source loading supports multiple files concatenated in order, and that diagnostics are still combined-source line/column.

`docs/language-grammar.ebnf` should not change because no language syntax is added.

## Out of Scope

- `import` statements.
- module namespaces.
- `export` declarations.
- file-aware diagnostics.
- dependency graph construction.
- cycle detection.
- path resolution beyond direct CLI arguments.
- separate compilation or incremental compilation.

## Future Work

After this phase, Phase 14B can add `import "path";` using the `SourceManager` as the loading foundation. That later phase should define relative import roots, duplicate import handling, cycle diagnostics, and file-aware diagnostics.
