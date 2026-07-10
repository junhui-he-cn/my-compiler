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

1. Add contextual lambda typing from an expected function type; do not attempt
   global parameter-type inference as part of this slice.

### M2: Module Ergonomics

1. Implement re-export as its own Phase 14E slice.
2. Design and implement search paths separately as Phase 14F, including CLI
   configuration, precedence, canonical paths, and diagnostics.
3. Define the module-interface metadata needed by future separate compilation,
   but do not implement a linker or separate compilation yet.

### M3: Language and Runtime Depth

1. Add parser recovery and multiple diagnostics.
2. Improve collection type inference.
3. Define a `.cdbc` version-compatibility policy.
4. Treat GC as a dedicated backend project; continue to defer task scheduling
   and JIT exploration.

The immediate dependency order is:

```text
contextual lambda typing
-> re-export
-> search paths
```

Each behavior-changing slice should start with a focused design spec and
implementation plan.

## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type
layer.

Future work:

- Improve collection inference while preserving mixed-array dynamic escape
  hatches.
- Add contextual typing for lambdas when an expected function type is available.
  Avoid global parameter-type inference until there is a stronger inference
  design.
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
- Define recursive struct type rules before allowing recursive fields.
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

- Re-export syntax for forwarding declarations from one module through another.
- Package or module search paths beyond explicit relative/absolute source paths.
- Stable module-interface metadata needed by possible future separate
  compilation. Do not implement a linker or separate compilation in the
  near-term roadmap.

Recommended split:

- Phase 14E: re-export syntax for forwarding declarations from one module
  through another, for example after deciding between `export name from
  "path";` and `export { name } from "path";`.
- Phase 14F: package/module search paths for less verbose imports, designed only
  after re-export semantics are stable. Specify CLI flags, resolution
  precedence, canonicalization, and failure diagnostics together.
- Phase 14G: define stable module-interface metadata needed by possible future
  separate compilation. Treat linker or separate-compilation implementation as a
  dedicated compiler/backend effort.

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
bytecode artifacts.

Future work:

- Phase 4A: define a `.cdbc` version-compatibility policy.
- Phase 4B: design GC heap ownership and root scanning as a dedicated backend
  project.
- Continue to defer task scheduling and JIT metadata/hot-path exploration.

Before starting a backend implementation phase, create a dedicated backend
design spec and implementation plan rather than mixing it into language feature
work.

## Near-Term Recommendation

Follow one dependency-driven sequence rather than choosing among parallel module,
type-system, and refactoring tracks:

```text
contextual lambda typing
-> Phase 14E re-export
-> Phase 14F search paths
```

Do not start a visitor rewrite, unified assignment AST, separate compilation,
additional nullable narrowing, GC, task scheduling, or JIT as part of these
near-term slices.
