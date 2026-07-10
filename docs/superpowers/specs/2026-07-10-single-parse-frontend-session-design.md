# Single-Parse Frontend Session Design

## Goal

Replace the current two-stage module-loading path with one front-end boundary
that owns source loading, parsing, import discovery, module identity, and
file-aware diagnostics. It does not add imported struct methods or change
module semantics. The intentional diagnostic-order change is that a module's
own lexer/parser failure is reported before recursively loading its imports.

## Current Problem

`SourceManager` discovers imports with a handwritten source-text scan. When
any import is found, `ModuleProgram` lexes and parses every loaded source unit
again. `main.cpp` also re-lexes units to decide whether to use module assembly
and remaps diagnostics from a combined source string for direct multi-file
inputs. These overlapping responsibilities create duplicate parsing and make
file-aware diagnostics depend on CLI-specific control flow.

## Design

Introduce `FrontendSession` as the only module front-end entry point for
file-backed and stdin input. It owns a collection of parsed module records.
Each record contains a stable module ID, display path, canonical path, source
text, parsed statements, scanned tokens, entry-file flag, and resolved import
edges.

For direct CLI inputs, the session performs a bounded lexer scan of their
newline-joined source and stops at the first `Import` token. If none appears,
that same complete token stream is parsed as the legacy combined source. If an
import appears, the session parses file-backed modules separately. This mode
selection preserves cross-file lexical syntax without reintroducing a
handwritten import directive scanner; resolved dependency edges still come
only from parsed `ImportStmt` nodes.

For a file-backed module, the session will:

1. Normalize and canonicalize its path.
2. Detect an import cycle from the active canonical-path stack, or reuse an
   already-loaded module.
3. Read, lex, and parse the source exactly once.
4. Wrap located lexer/parser diagnostics in `FileDiagnosticError` with that
   file's source context.
5. Inspect only parsed top-level `ImportStmt` nodes, resolve their requested
   paths relative to the importing file, recursively load dependencies, and
   assign `resolvedModuleId` on each import AST node.

Stdin is parsed once as an entry module. Any parsed top-level import from stdin
raises the existing locationless import diagnostic: `import is not supported
from stdin`.

## Program Assembly

The session returns a final `Program` while preserving the current split in
source semantics:

- If any loaded module contains imports, create the existing `ModuleStmt`
  wrappers from the stored parsed statements. Their module IDs, source text,
  paths, entry flags, and import IDs retain the current module/type-checker
  contract.
- If file inputs contain no imports, combine the already-scanned direct-input
  token streams and parse them once as one source. This preserves both their
  shared top-level scope and existing syntax that deliberately crosses a file
  boundary. Direct multi-file diagnostics are remapped to the owning file.
- A stdin program uses its parsed statements directly.

The session also exposes its stored tokens/source data for CLI output. `main`
will no longer scan for import tokens, invoke a separate module assembler, or
remap combined-source diagnostic locations.

## Ownership and Cleanup

`SourceManager` and `ModuleProgram` are removed if no remaining code consumes
them. Path normalization, file reading, import-stack tracking, and
canonical-path de-duplication move into `FrontendSession`; handwritten
`scanImportDirectives` and `containsTopLevelImportKeyword` do not survive.

This refactor deliberately leaves AST node definitions, `ImportStmt` fields,
`TypeChecker` module/export handling, IR lowering, bytecode lowering, and Rust
VM semantics unchanged.

## Error Handling and Compatibility

The following behavior remains stable:

- Relative import resolution, canonical duplicate-import suppression, and
  import-cycle detection.
- Existing message text and exit behavior for failed imports, cycles, and
  stdin imports.
- File-local lexer/parser/type diagnostics for imported files and direct
  multi-file inputs.
- Module-private top-level scopes, exports, direct imports, and namespace
  imports.
- AST, IR, bytecode, and C++/Rust runtime outputs for existing fixtures.

Imports are discovered from successfully parsed `ImportStmt` nodes, not from a
handwritten source scanner. Consequently, a lexer or parser failure in a
module is reported before a missing dependency referenced later in that same
module can be loaded. This replaces the legacy pre-scan diagnostic order and
is covered by a focused CLI regression test.

Direct input files that fail during parse are now diagnosed at their native
file location by the session rather than after parsing a combined source. The
visible diagnostic shape remains unchanged.

## Tests and Acceptance

Add focused regression coverage for loading a shared dependency through more
than one importer or spelling while preserving canonical de-duplication and
correct import IDs. Keep existing import, cycle, file-diagnostic, direct
multi-file, namespace-import, AST, IR, bytecode-artifact, and Rust VM parity
fixtures passing.

Acceptance requires the repository's full verification set:

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

## Non-Goals

- Imported or namespaced struct methods.
- Re-export syntax, package search paths, and separate compilation.
- Visitor conversion, TypeChecker subsystem extraction, or assignment-AST
  unification.
- Any language syntax, type rule, runtime, IR, bytecode, or Rust VM change.
