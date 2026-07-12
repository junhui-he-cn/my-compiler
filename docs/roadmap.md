# Compiler Design Language Roadmap

This roadmap is the active planning entry point for future user-visible language
and engineering work. Historical roadmap details are preserved in
`docs/roadmap-archive-2026-07-04.md` and the current pre-cleanup roadmap is
backed up in `docs/roadmap-backup-2026-07-10.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable. The
old in-process C++ bytecode VM path is gone; future backend work targets the
standalone Rust `compiler-design-vm` project under `vm-rs/` and `.cdbc`
artifacts.

For exact implemented grammar and user behavior, see
`docs/language-grammar.ebnf` and `README.md`.

## Guiding Principles

- Prefer vertical language slices that update parser, AST, type checker, IR,
  bytecode lowering, Rust VM behavior, docs, and goldens together when behavior
  crosses layers.
- Keep bytecode lowering and Rust VM execution aligned for every supported
  user-visible feature covered by execution tests.
- Keep planned syntax out of `README.md` and `docs/language-grammar.ebnf` until
  it exists.
- Write a focused design spec and implementation plan for each substantial phase
  before changing compiler behavior.
- Preserve parse errors, type errors, compile errors, import errors, and runtime
  errors as distinct test categories.

## Development Priority Bands

The front end remains the main scaling constraint. Prefer finishing existing
language semantics before adding broader module ergonomics or backend depth.

### M1: Complete Existing Language Semantics

No active M1 language semantics items remain.

### M2: Module Ergonomics

No active M2 module ergonomics items remain. Phase 14G module-interface metadata
is implemented as a stable `--module-interface` debug output. Linker and
separate-compilation work remain deferred.

### M3: Language Depth

1. Continue struct language polish.
2. Add focused standard builtin and collection helper slices.
3. Improve language polish and diagnostics.

The immediate dependency order is:

```text
struct language polish
```

Each behavior-changing slice should start with a focused design spec and
implementation plan. Backend and VM work is deferred and should not be mixed
into these near-term language slices.

## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type
layer.

Future work:

- No active near-term collection inference work remains after nullable-aware
  array literal merging and direct unannotated array mutation refinement.
- Do not plan loop-condition narrowing for `while` or conditional `for` bodies,
  post-branch simple-variable narrowing, or field/index nullable narrowing.

Likely touch points:

- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp`
- `include/Ast.hpp` and `src/Ast.cpp` if type syntax grows new AST nodes
- `include/Parser.hpp` and `src/Parser.cpp` for user-visible type syntax
- type-error golden fixtures
- `README.md`
- `docs/language-grammar.ebnf`

## Phase 12: Records / Structs

Goal: extend named structs from local static method calls to cross-module method
metadata and richer struct behavior.

Future work:

- Decide whether named runtime values should expose runtime type names beyond
  the current generic `struct` result.
- Recursive struct field types are explicitly rejected for now; revisit only with
  a dedicated design for recursive initialization and runtime representation.
- Decide whether field creation by assignment should exist; current assignment
  only mutates existing fields.
- Keep dynamic dispatch, inheritance, overloading, protocols, and optional
  chaining out of the near-term struct slice unless a dedicated design justifies
  them.

Likely touch points:

- struct method metadata in the type checker
- module export/import symbol metadata
- member-call type checking
- IR and bytecode lowering for method calls if call lowering changes
- Rust VM execution parity when runtime behavior changes
- docs and golden fixtures

## Phase 13: Standard Builtins

Goal: provide a small standard environment while keeping builtins shadowable
where practical and preserving bytecode/Rust VM parity.

Future work:

- Add additional collection helpers beyond current array mutation basics.
- Consider migrating legacy `len` lowering onto the generic native-call path if
  it simplifies the backend without changing user behavior.
- Define conventions for future standard-library functions: shadowing behavior,
  runtime validation, static checks for known types, and error messages.

Each builtin should define behavior for bytecode lowering and Rust VM execution,
with focused VM coverage for runtime behavior.

## Phase 14: Modules / Imports

Goal: improve cross-file ergonomics without introducing separate compilation too
early.

Future work:

- No active near-term module ergonomics work remains.
- Linker and separate-compilation implementation remain deferred to a dedicated
  future compiler/backend effort.

Recommended split:

- Phase 14G: stable module-interface metadata is complete. The compiler now
  supports `--module-interface` for inspecting each loaded module's exported
  static API.

Why late: modules affect diagnostics, CLI source management, test layout, and
name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Future work:

- Add parser recovery and multi-error reporting.
- Decide whether comments or doc comments need language-level documentation or
  additional syntax support.
- Do not plan additional nullable narrowing beyond the currently implemented
  direct `if` nil-check branch behavior.

## Code Health / Refactoring Backlog

Goal: keep feature work cheap by reducing duplication and oversized front-end
hotspots without changing language behavior or golden outputs.

Recommended future cleanup slices:

- **Add sanitizer guardrails.** Add opt-in ASan and UBSan configurations before
  larger front-end changes.
- **Consider a unified assignment AST only after more target forms appear.**
  Current separate nodes (`AssignExpr`, `IndexAssignExpr`, `FieldAssignExpr`,
  and their compound variants) are acceptable, but a future `AssignmentTarget`
  representation may reduce AST/type-checker/lowering duplication if assignment
  variants continue to grow.
- **Defer visitor-style AST dispatch until dynamic-cast chains become blocking.**
  `TypeChecker::checkExpressionInfo` and `IRCompiler::compileExpression` are
  long but still workable. A visitor or explicit `ExprKind` enum would be a
  larger architecture change and should be planned as its own behavior-preserving
  refactor.

## Deferred Backend Track

Future backend work targets the Rust `compiler-design-vm` project and `.cdbc`
bytecode artifacts, but this track is paused for the active roadmap.

Deferred work:

- Define a `.cdbc` version-compatibility policy.
- Design GC heap ownership and root scanning as a dedicated backend project.
- Continue to defer task scheduling and JIT metadata/hot-path exploration.

Do not start backend implementation from the active near-term queue. Before
resuming backend work, create a dedicated backend design spec and implementation
plan rather than mixing it into language feature work.

## Near-Term Recommendation

Follow one dependency-driven sequence rather than choosing among parallel module,
type-system, and refactoring tracks:

```text
struct language polish
```

Do not start a visitor rewrite, unified assignment AST, separate compilation,
additional nullable narrowing, `.cdbc` versioning, GC, task scheduling, or JIT as
part of these near-term language slices.
