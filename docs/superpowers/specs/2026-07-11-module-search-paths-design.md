# Module Search Paths Design

## Goal

Add Phase 14F module search paths so users can keep string-based imports while avoiding long relative paths for shared modules.

Example:

```sh
compiler_design -I stdlib --import-path vendor main.cd
```

```cd
import "math";
import "pkg/helpers";
export Point from "geometry";
```

The feature should extend source loading only. It should not add new import syntax, package metadata, runtime module objects, bytecode format changes, a linker, or separate compilation.

## User-Facing CLI

The CLI accepts search directories with both spellings:

```sh
-I DIR
--import-path DIR
```

Both options append one directory to the import search path. Multiple options are searched in the order they appear on the command line:

```sh
compiler_design -I first --import-path second main.cd
```

This searches `first` before `second` after the importing file's own directory has been tried.

Missing option arguments are command-line usage errors and should keep the existing CLI behavior of printing usage and exiting with status 64.

The options apply to all normal frontend modes, including AST printing, `--tokens`, `--ir`, `--bytecode`, and `--emit-bytecode`.

## Import Path Classification

The source loader classifies each string import path before resolving it.

Explicit paths are:

- paths beginning with `./`;
- paths beginning with `../`;
- absolute paths, as determined by `std::filesystem::path::is_absolute()`.

Non-explicit paths are all other import strings, such as:

- `math`;
- `pkg/math`;
- `pkg/math.cd`.

This classification is intentionally independent of whether a path contains directory separators. `pkg/math` can use search paths because it does not explicitly anchor itself with `./`, `../`, or an absolute root.

## Resolution Order

For explicit paths, resolution keeps the current import-relative base behavior
and adds only the shared `.cd` candidate generation:

1. Resolve relative to the directory of the file that contains the import, or as the absolute path if already absolute.
2. Try the raw string exactly.
3. If the raw string has no extension, try appending `.cd`.
4. Do not fall back to CLI search paths.

For non-explicit paths, resolution is:

1. Resolve relative to the directory of the file that contains the import.
2. If not found, resolve relative to each CLI search path in command-line order.
3. For each base directory, try the raw string exactly.
4. If the raw string has no extension, also try appending `.cd`.

For example, in `/project/app/main.cd` with `-I /project/stdlib`:

```cd
import "math";
```

The loader tries:

1. `/project/app/math`
2. `/project/app/math.cd`
3. `/project/stdlib/math`
4. `/project/stdlib/math.cd`

Current-file directory precedence is deliberate. It preserves existing import behavior and lets local files shadow search-path modules.

## Re-export Resolution

Re-export source clauses use the exact same resolver as ordinary imports:

```cd
export Point, origin from "geometry";
```

The resolver should be shared by ordinary `ImportStmt` handling and `ExportStmt::sourcePath` handling so precedence, `.cd` fallback, diagnostics, duplicate suppression, and cycle detection remain consistent.

## Stdin Behavior

Imports from stdin remain unsupported in this phase.

Even if `-I` or `--import-path` is provided, the loader should still reject ordinary imports and re-export source clauses when the entry source comes from stdin. This keeps the current requirement that imports have a real file as their relative base and avoids expanding the module model in this slice.

## Diagnostics

Explicit path failures should remain close to the existing diagnostic style. For example:

```text
Import error: failed to open import: ./missing
```

Non-explicit search failures should explain both the original import string and the candidates that were tried. A stable shape such as this is sufficient:

```text
Import error: failed to resolve import `math`; tried: /project/app/math, /project/app/math.cd, /project/stdlib/math, /project/stdlib/math.cd
```

The candidate list should use display paths that are useful to the user. Exact absolute-vs-relative spelling can follow existing display-path conventions as long as golden tests make it stable.

Parse, type, compile, and runtime diagnostic categories do not change. Search resolution failures are import errors.

## Canonicalization, Duplicates, and Cycles

After a candidate file is found, the loader should continue to canonicalize it before duplicate and cycle checks.

Consequences:

- `import "./lib.cd";` and `import "lib";` load only once if both resolve to the same canonical file.
- Two search paths that contain different files with the same module string produce distinct modules because their canonical paths differ.
- Import cycles through search-resolved modules use the existing cycle detection machinery.
- Re-export cycles through search-resolved sources use the same cycle diagnostics as ordinary imports.

## Architecture

### `main.cpp`

Extend CLI parsing to collect search paths:

- recognize `-I` followed by a directory;
- recognize `--import-path` followed by a directory;
- append each directory in command-line order;
- pass the vector to `FrontendSession` before loading stdin or files;
- update usage text.

No new frontend mode is needed.

### `FrontendSession`

Add search-path state and a small resolver boundary.

One possible shape:

```cpp
void setImportSearchPaths(std::vector<std::string> paths);

struct ImportResolution {
    std::filesystem::path path;
    std::vector<std::string> triedDisplayPaths;
};

ImportResolution resolveImportPath(
    const std::filesystem::path& importingFile,
    const Token& pathToken,
    bool allowSearchPaths);
```

The exact names can differ, but the responsibilities should stay centralized:

- classify explicit vs non-explicit strings;
- generate raw and `.cd` candidates;
- search the current file directory and then CLI paths when allowed;
- return the found path for `loadFile`;
- provide tried candidates for failed-resolution diagnostics.

Existing load order should stay intact: parse a unit, scan top-level imports and re-export source clauses, recursively load dependencies, assign module IDs, and assemble the final program.

### Parser, AST, TypeChecker, IR, Bytecode, Rust VM

Parser and AST syntax do not change.

Type checking, IR lowering, bytecode lowering, and Rust VM execution should not need semantic changes because search paths only affect which source file each import or re-export loads. Existing resolved module IDs should continue to connect later phases to the loaded modules.

## Tests

Add golden or focused tests for successful behavior:

1. `-I dir` resolves `import "math";` to `dir/math.cd`.
2. `--import-path dir` resolves the same case.
3. Multiple search paths are tried in command-line order.
4. The importing file's directory wins over search paths.
5. `import "pkg/math";` resolves through a search-path subdirectory.
6. An extension-bearing import such as `import "math.cd";` does not try `math.cd.cd`.
7. An explicit relative import such as `import "./local";` does not fall back to search paths.
8. Re-export source clauses resolve through search paths.
9. Duplicate canonical imports still load once when one spelling uses search paths.

Add error coverage for:

1. Missing `-I` argument: usage plus exit 64.
2. Missing `--import-path` argument: usage plus exit 64.
3. Non-explicit import missing everywhere: import error with tried candidates.
4. Explicit relative import missing: existing-style import error.
5. Stdin input with import and search paths: existing stdin import rejection.

Update bytecode and Rust VM parity tests only if a new successful fixture should be executed in those modes. The bytecode text format does not change.

## Documentation

Update `README.md` to document:

- `-I DIR` and `--import-path DIR`;
- current-file directory precedence;
- search-path order;
- raw path then `.cd` fallback;
- explicit `./`, `../`, and absolute paths not using search fallback;
- re-export source clauses using the same resolution rules;
- stdin imports remaining unsupported.

Update `docs/language-grammar.ebnf` only if a short comment is useful; grammar syntax itself remains unchanged.

Update `docs/roadmap.md` after implementation to mark Phase 14F complete and move the near-term recommendation to module-interface metadata.

## Non-goals

This phase does not add:

- non-string import syntax;
- config files;
- import maps;
- wildcard imports or wildcard re-exports;
- renaming imports or re-exports;
- package manifests;
- package versioning;
- separate compilation;
- a linker;
- runtime module values;
- bytecode format changes;
- imports from stdin.
