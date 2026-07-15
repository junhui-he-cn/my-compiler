# Compiler Design Language Roadmap

This roadmap is the active planning entry point for future user-visible language
and engineering work. Historical roadmap details are preserved in
`docs/roadmap-archive-2026-07-04.md` and the current pre-cleanup roadmap is
backed up in `docs/roadmap-backup-2026-07-10.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable. Future
backend work targets the standalone Rust `compiler-design-vm` project under
`vm-rs/` and `.cdbc` artifacts.

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

## Active Priorities

The front end remains the main scaling constraint. Focus near-term language work
on the remaining type-system and collection extensions, then on extensions to
pattern matching. Separate compilation and deeper backend work remain deferred
tracks.

Each behavior-changing slice should start with a focused design spec and
implementation plan. Compiler-pipeline, artifact, VM, and editor-tooling work
remain separate tracks and should not be mixed into the near-term language
slices.

## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type
layer.

The first generic-function slice now includes named functions, named-struct
methods, and anonymous function expressions. All three forms support inferred
and explicit call type arguments, while generic parameters are erased before
IR lowering.

Future work:

- Add constraints, generic container syntax beyond the built-in `map` form,
  and the inference rules needed by higher-order collection APIs.
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

Goal: continue struct polish around field rules and future type-system features
without expanding into object-system features prematurely.

Future work:
- Treat recursive struct field types as a separate design decision; do not add
  recursive initialization or runtime representation in an incremental polish
  slice.
- Keep field creation by assignment, dynamic dispatch, inheritance, overloading,
  protocols, and optional chaining out of the near-term struct work unless a
  dedicated design justifies them.

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

The first callback-based collection helpers, array `map` and `filter`, are
implemented through the existing generic `native_call` path. They support
shadowable function syntax and unshadowed member-call sugar, with static
callback/result element checking and Rust VM callback execution. `filter`
requires a boolean predicate and preserves the source element type.

Future work:

- Add further non-higher-order helpers only as focused slices with explicit
  mutation, shadowing, runtime-validation, static-checking, and error conventions.
- Defer callback-based helpers such as `reduce` until their mutation, callback,
  and accumulator conventions have a focused design.
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

- Linker and separate-compilation implementation remain deferred to a dedicated
  future compiler/backend effort.

Why late: modules affect diagnostics, CLI source management, test layout, and
name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Future work:

- Consider multi-error type reporting only as a separately designed recovery
  slice.
- Decide whether comments or doc comments need language-level documentation or
  additional syntax support.
- Do not add nullable narrowing beyond direct simple-variable `if` nil-checks
  and the supported logical-guard behavior.

## Phase 17: Collection Extensions

Goal: extend the existing collection types without committing prematurely to a
large general-purpose collection protocol.

Future work:

- Extend the first array `map`/`filter` slices with generic collection syntax
  and further higher-order APIs once their remaining inference boundaries are
  defined.
- Add further map operations, such as deletion, only as focused slices with
  explicit mutation and missing-key conventions.
- Add further range/collection operations only as focused slices with explicit
  mutation and bounds semantics.

## Phase 18: Pattern-Matching Extensions

Goal: extend the existing named-enum and statement-level pattern-matching
facilities without prematurely expanding the struct object model.

Future work:

- Match expressions, guards, named payload fields, generic enums, and nullable
  enum patterns remain out of scope.
- Defer inheritance, dynamic dispatch, and protocol/trait systems unless this
  phase identifies a concrete need for one.

## Code Health / Refactoring Backlog

Goal: keep feature work cheap by reducing duplication and oversized front-end
hotspots without changing language behavior or golden outputs.

Recommended future cleanup slices:

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

## Compiler Pipeline and Tooling Track

These directions are valuable for a compiler-design project, but are not
dependencies of the near-term language sequence.

- **IR and optimization:** add control-flow/basic-block inspection, IR
  validation, then behavior-preserving optimizations such as constant folding
  and dead-code elimination. Consider register allocation only after the IR
  invariants and measurement goals are clear.
- **Developer tools:** consider a formatter, REPL, language-server support, and
  source-level debugging as separate products with stable syntax and diagnostic
  requirements.
- **Robustness testing:** add lexer/parser and `.cdbc` parser fuzzing or
  property tests, especially around malformed source and artifacts.

## Deferred Backend Track

Future backend work targets the Rust `compiler-design-vm` project and `.cdbc`
bytecode artifacts, but this track is paused for the active roadmap.

Deferred work:

- Define a `.cdbc` version-compatibility policy.
- Design linker inputs and separate-compilation artifacts around module
  interfaces; package manifests and import maps are later product decisions, not
  prerequisites for a linker.
- Design GC heap ownership and root scanning as a dedicated backend project.
- Continue to defer task scheduling and JIT metadata/hot-path exploration.

Do not start backend implementation from the active near-term queue. Before
resuming backend work, create a dedicated backend design spec and implementation
plan rather than mixing it into language feature work.

## Near-Term Recommendation

Prioritize the remaining language extensions in this order:

```text
type-system extensions
-> collection extensions and higher-order APIs
-> pattern-matching extensions
```

Keep field creation by assignment, recursive struct fields, separate
compilation, additional nullable narrowing, `.cdbc` versioning, GC, task
scheduling, and JIT outside these near-term language slices.
