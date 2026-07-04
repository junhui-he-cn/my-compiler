# Compiler Demo Language Roadmap

This roadmap is the active planning entry point for user-visible language development. The previous mixed compiler/backend roadmap is preserved in `docs/roadmap-archive-2026-07-04.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable, but they are a deferred backend track. The current near-term direction is to improve the language itself.

## Current Implemented Baseline

The language currently supports:

- Statements: `let`, `print`, `if`/`else`, `while`, `fun`, `return`, blocks, and expression statements.
- Expressions: literals, arrays, indexing, variables, calls, function expressions, grouping, unary operators, binary/logical operators, and assignment expressions.
- Lexical scopes resolved during type checking.
- Explicit `let` annotations for `number`, `bool`, `string`, and `nil`.
- Named functions, anonymous function expressions, recursion, returns, and by-reference closures.
- Array literals and read-only indexing.
- IR interpreter and bytecode VM execution paths that should match for implemented language features.

For exact implemented grammar and user behavior, see `docs/language-grammar.ebnf` and `README.md`.

## Guiding Principles

- Prefer vertical language slices that update parser, AST, type checker, IR, bytecode lowering, interpreters/VM, docs, and goldens together when behavior crosses layers.
- Keep `--run` and `--run-bytecode` behavior aligned for every supported user-visible feature.
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

Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Function parameter types, function return types, function type annotations, and array element types remain future work.

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested future features:

- Full function signatures for named functions and function expressions, including parameter and return types.
- User-visible function type annotations, after choosing syntax.
- Array element types, after deciding whether mixed arrays remain a dynamic escape hatch.
- Basic inference for unannotated `let` declarations from initializer expressions.
- Static call checks for variables that are known functions.
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

## Phase 10: Array Mutation and Collection Builtins

Goal: make arrays useful beyond read-only literals and indexing.

Suggested features:

- `len(xs)` for arrays and strings, if string length should be included.
- Index assignment: `xs[i] = value`.
- `push(xs, value)` and possibly `pop(xs)` after choosing mutable array semantics.
- Runtime checks for non-array values, invalid indexes, and bounds.
- Static checks for known non-array values and known invalid index types.

Likely touch points:

- assignment target parsing and AST representation
- `Value` array representation if arrays become mutable
- IR operations for index assignment or builtin calls
- IR interpreter and bytecode VM behavior
- runtime-error and type-error fixtures
- success fixtures with `run.out` and `run_bytecode.out`

Recommended split:

- Phase 10A: `len` builtin as a small usability slice.
- Phase 10B: index assignment.
- Phase 10C: `push` / `pop` mutation helpers.

## Phase 11: Loop Control and For Loops

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

- Phase 11A: `break` / `continue` for existing `while` loops.
- Phase 11B: `for` loop syntax and lowering.

## Phase 12: Records / Structs

Goal: add named fields and simple aggregate data.

Possible approaches:

- Record literals first: `{ name: "Ada", age: 36 }`.
- Field access: `person.name`.
- Field assignment after mutation rules are clear: `person.age = 37`.
- Named structs later: `struct Person { name: string, age: number }`.

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

Suggested builtins:

- Numeric helpers: `floor`, `ceil`, `sqrt`.
- String helpers: `str`, `substr`, `charAt`.
- Collection helpers: `len`, `push`, `pop` if not completed in Phase 10.
- Debug helper: `typeOf` if useful for mixed runtime values.

Each builtin should define behavior for both the IR interpreter and bytecode VM paths, preferably through shared runtime machinery so semantics stay aligned.

## Phase 14: Modules / Imports

Goal: allow programs to be split across files.

Suggested features:

- `import "path";` or `import name from "path";` after selecting a module model.
- Deterministic path resolution relative to the importing file.
- Clear cycle handling.
- CLI behavior for multi-file source loading.
- Golden fixtures that include secondary source files.

Why late: modules affect diagnostics, CLI source management, test layout, and name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Suggested features:

- Source snippets and carets for front-end diagnostics.
- More parse recovery and multi-error reporting.
- Clear handling for lambda expression statements that begin with `fun`, either by documenting parenthesized form or changing parser disambiguation.
- Compound assignment operators such as `+=`, after assignment targets are generalized.
- Comments or doc comments if they are still missing.

## Deferred Backend Track

The bytecode VM already exists and provides extension points for backend research. These directions are deferred while the active roadmap focuses on language features:

- GC groundwork: VM heap ownership, root scanning, and value reachability.
- Task scheduling: schedulable VM threads, instruction budgets, yield points, and blocked states.
- JIT exploration: bytecode metadata, hot function detection, and native-code experiments.

Before starting any backend track, create a dedicated backend design spec and implementation plan rather than mixing it into this language roadmap.

## Near-Term Recommendation

Start with **Phase 9: Richer Type System** if the priority is stronger foundations for records, mutable arrays, and builtin APIs.

Choose **Phase 10A: `len` builtin** instead if the priority is a small, immediately visible usability improvement before deeper type-system work.
