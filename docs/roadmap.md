# Compiler Design Language Roadmap

This roadmap is the active planning entry point for user-visible language development. The previous mixed compiler/backend roadmap is preserved in `docs/roadmap-archive-2026-07-04.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable. The former C++ bytecode VM has been removed, and future backend work targets the standalone Rust `compiler-design-vm` project under `vm-rs/` and `.cdbc` artifacts. The current near-term language direction remains improving the language itself.

Deferred backend milestone: Phase 3B removed the old C++ bytecode VM and its in-process bytecode-run CLI mode; Rust VM is now the bytecode execution backend. Implemented.

## Current Implemented Baseline

The language currently supports:

- Statements: `let`, `print`, `if`/`else`, `while`, `break`, `continue`, `fun`, `return`, top-level `import`, blocks, and expression statements.
- Expressions: literals, arrays, indexing, array index assignment, structs, field access, field assignment, variables, calls, function expressions, grouping, unary operators, binary/logical operators, and assignment expressions.
- Lexical scopes resolved during type checking.
- Explicit `let` annotations for `number`, `bool`, `string`, and `nil`.
- Named functions, anonymous function expressions, recursion, returns, and by-reference closures.
- Array literals, indexing, and array index assignment.
- Anonymous struct literals, field access, and existing-field assignment.
- C++ IR interpreter execution via `--run`, plus bytecode artifact emission and Rust VM execution via `.cdbc`.

For exact implemented grammar and user behavior, see `docs/language-grammar.ebnf` and `README.md`.

## Guiding Principles

- Prefer vertical language slices that update parser, AST, type checker, IR, bytecode lowering, interpreters/VM, docs, and goldens together when behavior crosses layers.
- Keep `--run`, bytecode lowering, and Rust VM execution aligned for every supported user-visible feature covered by parity tests.
- Keep planned syntax out of `README.md` and `docs/language-grammar.ebnf` until implemented.
- Write a focused design spec and implementation plan for each substantial phase before changing compiler behavior.
- Preserve parse errors, type errors, compile errors, and runtime errors as distinct test categories.

## Recommended Language Phase Order

```text
9. Richer type system
10. Array mutation and collection builtins
11. Loop control and for loops
12. Records / structs
13. Standard builtins
14. Modules / imports
15. Language polish and diagnostics
```

## Phase 9: Richer Type System

Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Phase 9C is implemented: known function values carry conservative inferred return types for call-result checking. Phase 9D is implemented: named functions and function expressions support optional parameter and return annotations for `number`, `bool`, `string`, and `nil`. Phase 9E is implemented: function type annotations use `fun(...): ...` syntax and support static signature checks for annotated variables, parameters, returns, assignments, and calls. Array element types remain future work.

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested future features:

- Array element types, after deciding whether mixed arrays remain a dynamic escape hatch.
- Deeper inference for currently unknown function parameters and call results.
- A clear compatibility rule for `nil`.

Likely touch points:

- `include/TypeChecker.hpp`
- `src/TypeChecker.cpp`
- `include/Ast.hpp` and `src/Ast.cpp` if type syntax grows new AST nodes
- `include/Parser.hpp` and `src/Parser.cpp` for user-visible type syntax
- type-error golden fixtures
- `README.md`
- `docs/language-grammar.ebnf`

Completed first slice: simple unannotated `let` types are inferred from known initializer expressions without introducing new type annotation syntax.

Completed second slice: known function values carry arity through function expressions, named function variables, same-arity assignments, and direct lambda calls.

Completed third slice: function bodies infer conservative return types, and calls through known function values feed those types into `let` inference and assignment checks.

Completed fourth slice: named functions and function expressions accept optional parameter and return annotations, use annotated parameter types in function bodies, and check explicit or implicit returns against annotated return types.

Completed fifth slice: function type annotations use `fun(type, ...): type` syntax in `let`, parameter, and return annotations, with static signature checks for assignments, calls, and returns.

## Phase 10: Array Mutation and Collection Builtins

Status: implemented for the current collection slice. Phase 10A is implemented: `len(value)` returns array element counts or string byte lengths with IR and bytecode parity. Phase 10B is implemented: `array[index] = value` mutates shared array elements and works in both runtime paths. Phase 10C is implemented: `push(array, value)` and `pop(array)` are shadowable native stdlib functions backed by a generic `native_call` IR/bytecode path. `push` mutates in place and returns `nil`; `pop` mutates in place and returns the removed value.

Goal: make arrays useful beyond read-only literals and indexing.

Suggested features:

- `len(xs)` for arrays and strings, if string length should be included.
- Index assignment: `xs[i] = value`.
- `push(xs, value)` and `pop(xs)` as in-place native stdlib helpers. Implemented.
- Runtime checks for non-array values, invalid indexes, and bounds.
- Static checks for known non-array values and known invalid index types.

Likely touch points:

- assignment target parsing and AST representation
- `Value` array representation if arrays become mutable
- IR operations for index assignment or builtin calls
- IR interpreter, bytecode lowering, and Rust VM behavior
- runtime-error and type-error fixtures
- success fixtures with `run.out` plus Rust VM parity coverage

Recommended split:

- Phase 10A: `len` builtin as a small usability slice. Implemented.
- Phase 10B: index assignment. Implemented.
- Phase 10C: `push` / `pop` mutation helpers. Implemented as function-style helpers
  (`push(xs, value)` / `pop(xs)`) before method syntax.
  Defer method-style syntax (`xs.push(value)` / `xs.pop()`) to Phase 12, where
  dot access and aggregate/member syntax can be designed together.

## Phase 11: Loop Control and For Loops

Status: in progress. Phase 11A is implemented: `break;` exits the nearest `while`, and `continue;` skips to the nearest `while` condition check. `for` loop syntax remains future work.

Goal: make iteration practical and structured.

Suggested features:

- `break;` exits the nearest loop.
- `continue;` starts the next nearest loop iteration.
- Type errors for `break` and `continue` outside loops.
- A later `for` form after array iteration and mutation semantics are clearer.

Likely touch points:

- `include/Token.hpp` and `src/Lexer.cpp` for new keywords.
- `include/Ast.hpp` and `src/Ast.cpp` for break/continue statements.
- `include/Parser.hpp` and `src/Parser.cpp` for statement parsing.
- `src/TypeChecker.cpp` for loop-depth validation.
- IR control-flow lowering and bytecode parity.
- parse/type/run golden fixtures.

Recommended split:

- Phase 11A: `break` / `continue` for existing `while` loops. Implemented.
- Phase 11B: `for` loop syntax and lowering.

## Phase 12: Records / Structs

Status: in progress. Phase 12A is implemented: anonymous struct literals and dot field access work across C++ `--run`, bytecode artifacts, and the Rust VM. Phase 12B is implemented: existing-field assignment `object.field = value` mutates shared struct fields and returns the assigned value across both runtime paths. Phase 12C is implemented: named struct declarations define static field shapes, named struct annotations check exact literal initialization, and known named struct field access/assignment is statically checked. Constructor syntax, methods, recursive structs, runtime type names, field creation by assignment, and richer struct type features remain future work.

Goal: add named fields and simple aggregate data.

Possible approaches:

- Struct literals first: `{ name: "Ada", age: 36 }`. Implemented.
- Field access: `person.name`. Implemented.
- Field assignment after mutation rules are clear: `person.age = 37`. Implemented for existing fields.
- Dot/member call syntax for collection methods such as `xs.push(value)` and
  `xs.pop()`, if method-style collection APIs are still desired.
- Named structs: `struct Person { name: string, age: number }`. Implemented as static-only type shapes.

Keep methods, inheritance, and protocols out of the first records slice.

Likely touch points:

- lexer/parser support for field syntax and dot access
- AST expression nodes for record literals and field access
- runtime value representation for records
- type checker field tracking
- IR and bytecode operations for field reads/writes
- docs and golden fixtures

## Phase 13: Standard Builtins

Goal: provide a small standard environment without introducing modules yet.

Status: in progress. Phase 13A is implemented: `floor(number)`, `ceil(number)`, and `sqrt(number)` are shadowable native stdlib functions using the generic `native_call` path. `len` remains supported through its legacy dedicated IR/bytecode opcode and still awaits migration if a unified builtin path becomes valuable.

Suggested builtins:

- Numeric helpers: `floor`, `ceil`, `sqrt`. Implemented.
- String helpers: `str`, `substr`, `charAt`.
- Collection helpers: `len` plus additional helpers beyond the Phase 10 `push`/`pop` slice.
- Debug helper: `typeOf` if useful for mixed runtime values.

Each builtin should define behavior for both the IR interpreter and bytecode artifact/Rust VM paths, preferably through shared runtime machinery so semantics stay aligned.

## Phase 14: Modules / Imports

Status: in progress. Phase 14A is implemented: the CLI accepts multiple input files and compiles them as one combined source in command-line order. Phase 14B is implemented: `import "path";` recursively expands source files relative to the importing file, suppresses duplicate canonical imports, reports missing-file/cycle/stdin import errors, and has bytecode/Rust VM parity coverage. Namespaces, exports, package search paths, separate compilation, and file-aware diagnostics remain future work.

Goal: allow programs to be split across files.

Suggested features:

- `import "path";` source loading. Implemented.
- Deterministic path resolution relative to the importing file. Implemented.
- Clear cycle handling. Implemented.
- CLI behavior for multi-file source loading. Implemented for direct CLI inputs.
- Golden fixtures that include secondary source files.

Why late: modules affect diagnostics, CLI source management, test layout, and name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Status: in progress. Phase 15A is implemented: located front-end diagnostics print the combined-source line and a caret while keeping the existing first diagnostic line stable. File-aware diagnostic remapping remains future work.

Suggested features:

- Source snippets and carets for front-end diagnostics. Implemented for combined-source locations.
- More parse recovery and multi-error reporting.
- Clear handling for lambda expression statements that begin with `fun`, either by documenting parenthesized form or changing parser disambiguation.
- Compound assignment operators such as `+=`, after assignment targets are generalized.
- Comments or doc comments if they are still missing.

## Deferred Backend Track

The old C++ bytecode VM has been removed. Future backend work targets the Rust `compiler-design-vm` project and `.cdbc` bytecode artifacts:

- Phase 0: rename to Compiler Design, scaffold `vm-rs/`, and document the planned `.cdbc` text format. Implemented.
- Phase 1: add a C++ `.cdbc` bytecode artifact emitter. Implemented.
- Phase 2: add Rust VM `.cdbc` parser and dump support. Implemented.
- Phase 3: add Rust VM executor parity for current bytecode semantics. Implemented.
- Phase 4: explore GC heap ownership/root scanning, task scheduling, and JIT metadata/hot paths.

Before starting a backend implementation phase, create a dedicated backend design spec and implementation plan rather than mixing it into language feature work.

## Near-Term Recommendation

Start with **Phase 9: Richer Type System** if the priority is stronger foundations for records, mutable arrays, and builtin APIs.

Choose **Phase 13: Standard Builtins** if the priority is expanding the native stdlib foundation beyond the implemented `push` / `pop` and `floor` / `ceil` / `sqrt` helpers.
