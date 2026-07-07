# Module Exports Design

Date: 2026-07-07

## Goal

Implement the next Modules / Imports slice by turning current source-loading imports into module imports with private module scope and explicit exported declarations.

The feature should let programs split helper code across files without leaking every imported top-level name into the importer. It should preserve the existing `import "path";` syntax and keep runtime execution aligned between C++ `--run`, emitted bytecode artifacts, and the Rust VM.

## Non-goals

This slice does not add:

- `import ... as name` or namespace-qualified access.
- `from` imports or selective import lists.
- `export import`, re-export lists, or wildcard exports.
- Package search paths.
- Separate compilation or cached module artifacts.
- File-aware diagnostic remapping beyond existing combined-source behavior.
- Runtime module values.

## User-facing syntax

Add `export` as a declaration modifier:

```cd
export let value = 1;
export fun add(a, b) { return a + b; }
export struct Point { x: number, y: number }
```

Grammar shape:

```ebnf
declaration = importDecl
            | exportDecl
            | structDecl
            | funDecl
            | letDecl
            | statement ;

exportDecl = "export", ( structDecl | funDecl | letDecl ) ;
```

Rules:

- `export` is only valid at file/module top level.
- `export` may only wrap `let`, `fun`, and `struct` declarations.
- `export print ...`, `export if ...`, `export while ...`, `export import ...`, and `export { ... }` are invalid.
- `export` inside blocks or function bodies is invalid.

The AST printer should expose the modifier with a stable wrapper form such as:

```text
(export (let value 1))
(export (fun add ...))
(export (struct Point ...))
```

## Module semantics

Each loaded file is a module-like source unit:

- A file's top-level declarations are visible to other top-level declarations in that same file.
- Non-exported top-level declarations are private to the file.
- Exported top-level declarations are added to the file's export table.
- `import "./lib.cd";` introduces the imported file's exported declarations into the importing file's top-level scope.
- Importing a file does not expose that file's private declarations to the importer.
- Imported exports may be used by declarations that appear after the import directive in the importer.
- Imported exports are not visible before the import directive; import order continues to matter as it does in the current source-loading model.

Example:

```cd
// lib.cd
let secret = 40;
fun inc(x) { return x + 1; }
export fun answer() { return inc(secret) + 1; }

// main.cd
import "./lib.cd";
print answer(); // 42
print secret;   // type error: undefined variable
```

CLI multi-file entry behavior remains backward-compatible for direct input files: files passed on the command line still form one entry compilation unit in command-line order. Imported dependency files use module privacy/export rules.

## Name conflicts

The module resolver should report front-end type/name errors for ambiguous or duplicate imported names:

- If two imports in the same file export the same name, report a duplicate declaration error.
- If an imported export conflicts with an existing top-level declaration in the importing file, report a duplicate declaration error.
- If an imported export conflicts with a later top-level declaration in the importing file, report a duplicate declaration error.
- Re-importing the same canonical file remains a no-op and should not create duplicate exported bindings.

Shadowing inside nested block scopes remains governed by existing lexical-scope rules.

## Source loading architecture

Replace the current pure text-splicing import model with structured source units.

`SourceManager` should keep responsibility for:

- Reading CLI input files.
- Resolving import paths relative to the importing file.
- Detecting missing imports and import cycles.
- Suppressing duplicate canonical imports.
- Rejecting imports from stdin.
- Recording source text and lightweight file metadata for future diagnostics.

Instead of returning only one fully expanded source string, `SourceManager` should expose a load result with ordered source units and import edges. The parser/type checker can then parse each file independently and preserve module boundaries.

A practical first implementation may keep a compatibility combined-source string for current CLI diagnostic formatting, but module visibility must be based on structured file units rather than raw textual inclusion.

## Parser and AST

Parser changes:

- Lex `export` as a keyword.
- Add `ExportStmt` or equivalent wrapper node around the exported declaration.
- Parse `export` only in declaration contexts where top-level status is known.
- Reject invalid export targets with stable parse errors.
- Preserve current parser handling for malformed import declarations that survive source loading.

Recommended AST shape:

```cpp
struct ExportStmt : Stmt {
    StmtPtr declaration;
};
```

This keeps `let`, `fun`, and `struct` declaration nodes mostly unchanged and centralizes export printing and validation.

## Name resolution and type checking

Introduce a module-resolution phase before existing IR lowering:

1. Parse each source unit into an AST.
2. Build a per-module top-level symbol table.
3. Validate export wrappers and collect each module's export table.
4. Resolve imports in source order by adding exported symbols from imported modules to the importing module's top-level scope at each import directive.
5. Type check each module with its private declarations plus imported exported declarations visible.

The resolver should map every top-level declaration to a unique internal binding identity. Importers should refer to the imported declaration's identity, not a textually duplicated name.

This phase can be integrated into the existing `TypeChecker` if that is the smallest safe change, but it should keep module-boundary logic isolated enough to support future namespace imports and file-aware diagnostics.

## IR, bytecode, and VM behavior

Module boundaries are compile-time only in this slice.

Final lowering still produces one program:

- Compile private top-level declarations because exported functions may depend on them.
- Compile exported declarations once, even if imported by multiple files.
- Lower all variable references to their resolved binding identities.
- Use unique internal names for module-private top-level declarations to avoid cross-module collisions, for example derived from canonical module id plus local name.

No new runtime bytecode module opcode or Rust VM module object is required. Existing IR, bytecode lowering, and VM execution should continue to work once front-end name resolution produces a unified program with unique bindings.

## Diagnostics

New diagnostics should preserve the existing diagnostic categories:

- Invalid `export` syntax: parse error.
- `export` in non-top-level positions: parse error if parser context can catch it cleanly; otherwise type error.
- Duplicate imported/exported names: type error.
- Access to non-exported imported private names: existing undefined-variable type error.
- Import loading failures and cycles: existing import errors.

Located diagnostics may continue to use combined-source line/column behavior until file-aware remapping is implemented in a later phase.

## Test plan

Add golden success fixtures for:

- Importing exported `let`, `fun`, and `struct` declarations.
- Exported function using private helper declarations in the same module.
- Private helper declarations used through an exported function, proving private declarations are compiled without becoming importer-visible.
- Duplicate canonical imports remaining no-op.
- CLI direct multi-file behavior staying compatible.

Add type-error fixtures for:

- Importer reads a non-exported private declaration.
- Two different imports export the same name.
- Imported export conflicts with a local top-level declaration.

Add parse-error fixtures for:

- `export print ...`.
- `export import "...";`.
- `export` inside a block.
- `export` missing a declaration.

Add bytecode/Rust VM parity coverage for at least:

- Exported function calling a private helper.
- Exported struct type annotation and constructor use.

Run the relevant subset during development and the full repository verification before completion:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

## Documentation updates

Update:

- `docs/language-grammar.ebnf` with `exportDecl`.
- `README.md` with module export syntax, visibility rules, and limitations.
- `docs/roadmap.md` to record Phase 14C status.
- `AGENTS.md` current language semantics for module exports.

## Open implementation choices

The implementation plan should decide exact internal interfaces for:

- SourceManager load-result structure and import graph representation.
- Whether module resolution lives in `TypeChecker` or a small separate resolver component.
- How resolved top-level binding identities are represented and passed to IR lowering.
- How combined-source diagnostics are preserved while parsing individual source units.

These choices are implementation details; they must not change the user-visible semantics described above.
