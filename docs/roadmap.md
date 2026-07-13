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

1. Add focused, non-higher-order collection helpers.
2. Add source locations and call stacks to runtime diagnostics.
3. Define Unicode string semantics and make string operations consistent with it.
4. Add generic type abstraction before exposing strongly typed higher-order
   collection APIs.
5. Add maps, ranges, and broader `for-in` iteration on top of that collection
   type foundation.
6. Consider algebraic data types and pattern matching as the next data-modeling
   layer, including the decision on recursive data.

The immediate dependency order is:

```text
small collection helpers
-> runtime diagnostics
-> Unicode strings
-> generics
-> maps, ranges, and iteration
-> enums and pattern matching
```

Each behavior-changing slice should start with a focused design spec and
implementation plan. Compiler-pipeline, artifact, VM, and editor-tooling work
remain separate tracks and should not be mixed into these near-term language
slices.

## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type
layer.

Future work:

- Design generic type abstraction for reusable collection and function APIs.
  Decide type-parameter syntax, generic function inference boundaries, and the
  relation between existing dynamic arrays and typed collections before adding
  higher-order builtins.
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

Current status:

- Named constructor-created struct values expose their attached runtime struct name
  through `typeOf`.
- Source-level anonymous struct literals are removed; users construct only
  declared named structs with `Name { ... }` or `alias.Name { ... }`.
- Struct methods are available on exported/imported named structs through direct,
  namespace, and re-export imports.

Future work:
- No active near-term struct language polish slice remains.
- Recursive struct field types remain unsupported. Do not plan recursive
  initialization or runtime representation without a future dedicated decision.
- Field creation by assignment remains unsupported; assignment only mutates
  declared existing fields.
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

- Start with a small set of non-higher-order collection helpers. Define each
  helper's mutation, shadowing, runtime-validation, static-checking, and error
  conventions before selecting the concrete API.
- Defer callback-based helpers such as `map`, `filter`, and `reduce` until
  generic function and collection types have a focused design.
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

- Parser recovery and multi-error Parse reporting are implemented. Extend
  diagnostics next with source locations and call stacks for runtime failures;
  consider multi-error type reporting only as a separately designed recovery
  slice.
- Decide whether comments or doc comments need language-level documentation or
  additional syntax support.
- Do not plan additional nullable narrowing beyond the currently implemented
  simple-variable `if` nil-check and logical-guard behavior.

## Phase 16: Unicode Strings

Goal: make string behavior predictable for non-ASCII source programs.

Future work:

- Choose and document whether language string indexing and length use Unicode
  scalar values, grapheme clusters, or intentionally remain byte-oriented.
- If changing the current byte-oriented behavior, update `len`, `substr`, and
  `charAt` together, including bounds diagnostics and Rust VM parity.
- Keep text normalization, locale-sensitive collation, and regex support out of
  the initial Unicode slice.

## Phase 17: Generic Collections and Iteration

Goal: establish a reusable, statically meaningful collection layer rather than
growing an array-only helper list.

Future work:

- Add generic collection and function typing only after the Phase 9 design
  specifies inference boundaries and dynamic-array escape hatches.
- Add maps/dictionaries with explicit key equality and iteration semantics.
- Add ranges and extend `for-in` beyond arrays only after collection iteration
  behavior is defined consistently for arrays, strings, maps, and ranges.
- Do not introduce a general custom-iterator protocol in the first slice.

## Phase 18: Algebraic Data Types and Pattern Matching

Goal: support values with explicit alternatives, such as success/failure or
tree-shaped data, without prematurely expanding the struct object model.

Future work:

- Design named variants/enums and exhaustive pattern matching together.
- Decide recursive type representation as part of this phase rather than
  enabling recursive struct fields in isolation.
- Defer inheritance, dynamic dispatch, and protocol/trait systems unless this
  phase identifies a concrete need for one.

## Code Health / Refactoring Backlog

Goal: keep feature work cheap by reducing duplication and oversized front-end
hotspots without changing language behavior or golden outputs.

Recommended future cleanup slices:

- **Maintain sanitizer guardrails.** Opt-in ASan and UBSan configurations and
  a sanitizer CI job are implemented; keep them passing as front-end work
  evolves.
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
  property tests, especially around malformed artifacts and source locations.

## Deferred Backend Track

Future backend work targets the Rust `compiler-design-vm` project and `.cdbc`
bytecode artifacts, but this track is paused for the active roadmap.

Deferred work:

- Define a `.cdbc` version-compatibility policy.
- Design linker inputs and separate-compilation artifacts around the existing
  module-interface metadata; package manifests and import maps are later
  product decisions, not prerequisites for a linker.
- Design GC heap ownership and root scanning as a dedicated backend project.
- Continue to defer task scheduling and JIT metadata/hot-path exploration.

Do not start backend implementation from the active near-term queue. Before
resuming backend work, create a dedicated backend design spec and implementation
plan rather than mixing it into language feature work.

## Near-Term Recommendation

Follow one dependency-driven sequence rather than choosing among parallel module,
type-system, and refactoring tracks:

```text
small collection helpers
-> runtime diagnostics
-> Unicode strings
-> generic collection types
-> maps, ranges, and iteration
-> enums and pattern matching
```

Do not start field creation by assignment, recursive struct field types, a visitor
rewrite, unified assignment AST, separate compilation, additional nullable
narrowing, higher-order collection helpers before generics, `.cdbc` versioning,
GC, task scheduling, or JIT as part of these near-term language slices.
