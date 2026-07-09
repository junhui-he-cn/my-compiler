# Compiler Design Language Roadmap

This roadmap is the active planning entry point for user-visible language development. The previous mixed compiler/backend roadmap is preserved in `docs/roadmap-archive-2026-07-04.md`.

Backend VM follow-ups such as GC, task scheduling, and JIT remain valuable. The former C++ bytecode VM has been removed, and future backend work targets the standalone Rust `compiler-design-vm` project under `vm-rs/` and `.cdbc` artifacts. The current near-term language direction remains improving the language itself.

Deferred backend milestone: Phase 3B removed the old C++ bytecode VM and its in-process bytecode-run CLI mode; Rust VM is now the bytecode execution backend. Implemented.

## Current Implemented Baseline

The language currently supports:

- Statements: `let`, `print`, `if`/`else`, `while`, `break`, `continue`, `fun`, `return`, top-level `import`, blocks, and expression statements.
- Expressions: literals, arrays, indexing, array index assignment, structs, field access, field assignment, variables, calls, builtin member calls, function expressions, grouping, unary operators, binary/logical operators, assignment expressions, and numeric variable compound assignment.
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

## Near-Term Development Queue

The next recommended language slices are ordered by usefulness, implementation
risk, and how well they build on recently completed work:

1. **Phase 14E: module re-export and search paths** — revisit modules after
   the core language ergonomics above are stronger.
2. **Phase 15E: index/field compound assignment** — extend compound assignment
   beyond numeric variables if mutation ergonomics remain a priority.

Each slice should still start with a focused design spec and implementation
plan before changing compiler behavior.

## Phase 9: Richer Type System

Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Phase 9B is implemented: known function values carry arity for static argument-count checks. Phase 9C is implemented: known function values carry conservative inferred return types for call-result checking. Phase 9D is implemented: named functions and function expressions support optional parameter and return annotations for `number`, `bool`, `string`, and `nil`. Phase 9E is implemented: function type annotations use `fun(...): ...` syntax and support static signature checks for annotated variables, parameters, returns, assignments, and calls. Phase 9F is implemented: array type annotations use `[type]` syntax and known element types flow through array literals, indexing, index assignment, `push`, and `pop`. Phase 9G is implemented: nullable annotations use postfix `?`, allowing `nil` only where `T?` is expected while keeping flow-sensitive narrowing as future work.

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested future features:

- Deeper collection inference while preserving mixed-array dynamic escape hatches.
- Deeper inference for currently unknown function parameters and call results.
- Flow-sensitive nullable narrowing after checks such as `x != nil`.

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

Completed sixth slice: array type annotations use `[type]` syntax in `let`, parameter, return, and struct field annotations. Known array element types are checked for literals, indexing, index assignment, `push`, and `pop`, while mixed unannotated arrays remain a dynamic escape hatch.

Completed seventh slice: nullable annotations use postfix `?`, so `nil` is assignable to `T?` while non-nullable `T` remains protected. Flow-sensitive narrowing remains future work.

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

Status: implemented for the current loop slice. Phase 11A is implemented: `break;` exits the nearest `while`, and `continue;` skips to the nearest `while` condition check. Phase 11B is implemented: C-style `for` loops support optional initializer, condition, and increment clauses, with `continue;` running the increment before the next condition check. Phase 11C is implemented: `for item in array { ... }` iterates arrays with a body-scoped item binding, length snapshot semantics, and `break` / `continue` support.

Goal: make iteration practical and structured.

Suggested features:

- `break;` exits the nearest loop.
- `continue;` starts the next nearest loop iteration.
- Type errors for `break` and `continue` outside loops.
- C-style `for` loop syntax and lowering. Implemented.
- Array `for-in` loop syntax and lowering. Implemented.

Likely touch points:

- `include/Token.hpp` and `src/Lexer.cpp` for new keywords.
- `include/Ast.hpp` and `src/Ast.cpp` for break/continue statements.
- `include/Parser.hpp` and `src/Parser.cpp` for statement parsing.
- `src/TypeChecker.cpp` for loop-depth validation.
- IR control-flow lowering and bytecode parity.
- parse/type/run golden fixtures.

Recommended split:

- Phase 11A: `break` / `continue` for existing `while` loops. Implemented.
- Phase 11B: C-style `for` loop syntax and lowering. Implemented.
- Phase 11C: array `for-in` iteration. Implemented for arrays only; strings, maps, ranges, and custom iterators remain out of scope.

## Phase 12: Records / Structs

Status: in progress. Phase 12A is implemented: anonymous struct literals and dot field access work across C++ `--run`, bytecode artifacts, and the Rust VM. Phase 12B is implemented: existing-field assignment `object.field = value` mutates shared struct fields and returns the assigned value across both runtime paths. Phase 12C is implemented: named struct declarations define static field shapes, named struct annotations check exact literal initialization, and known named struct field access/assignment is statically checked. Phase 12D is implemented: named struct constructor expressions `Name { ... }` infer named struct types while reusing anonymous runtime struct values. Phase 12E is implemented: builtin member-call sugar supports selected array/string helpers (`push`, `pop`, `len`, `substr`, `charAt`) while full user-defined methods remain future work. Methods, recursive structs, runtime type names, field creation by assignment, and richer struct type features remain future work.

Goal: add named fields and simple aggregate data.

Possible approaches:

- Struct literals first: `{ name: "Ada", age: 36 }`. Implemented.
- Field access: `person.name`. Implemented.
- Field assignment after mutation rules are clear: `person.age = 37`. Implemented for existing fields.
- Builtin member-call sugar for collection/string helpers such as `xs.push(value)`,
  `xs.pop()`, `xs.len()`, and `text.substr(start, length)`. Implemented.
- Named structs: `struct Person { name: string, age: number }`. Implemented as static-only type shapes with `Person { ... }` constructor expressions.
- Full user-defined methods, `this`, struct methods, and optional chaining remain future work.

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

Status: in progress. Phase 13A is implemented: `floor(number)`, `ceil(number)`, and `sqrt(number)` are shadowable native stdlib functions using the generic `native_call` path. A string helper slice is implemented with `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` on the same shadowable native-call path. Phase 13C is implemented: `typeOf(value)` returns runtime type names as strings on the shadowable native-call path. `len` remains supported through its legacy dedicated IR/bytecode opcode and still awaits migration if a unified builtin path becomes valuable.

Suggested builtins:

- Numeric helpers: `floor`, `ceil`, `sqrt`. Implemented.
- String helpers: `str`, `substr`, `charAt`. Implemented.
- Collection helpers: `len` plus additional helpers beyond the Phase 10 `push`/`pop` slice.
- Debug helper: `typeOf`. Implemented for runtime type names (`nil`, `number`, `bool`, `string`, `function`, `array`, and `struct`).

Each builtin should define behavior for both the IR interpreter and bytecode artifact/Rust VM paths, preferably through shared runtime machinery so semantics stay aligned.

## Phase 14: Modules / Imports

Status: in progress. Phase 14A is implemented: the CLI accepts multiple input files and compiles them as one combined source in command-line order. Phase 14B is implemented: `import "path";` recursively loads source files relative to the importing file, suppresses duplicate canonical imports, reports missing-file/cycle/stdin import errors, and has bytecode/Rust VM parity coverage. Phase 14C is implemented: imported files have module-private top-level scope, and standalone export lists expose selected already-defined top-level declarations to importers while keeping private helpers hidden. Phase 14D is implemented: namespace imports with `import "path" as name;` provide qualified access to exported values, functions, and structs without top-level name pollution. Package search paths, separate compilation, and re-export syntax remain future work.

Goal: allow programs to be split across files.

Suggested features:

- `import "path";` source loading. Implemented.
- Deterministic path resolution relative to the importing file. Implemented.
- Clear cycle handling. Implemented.
- Standalone `export name[, name...];` lists for explicit cross-file visibility. Implemented.
- Namespace imports with `import "path" as name;` for qualified access to exported values, functions, and structs. Implemented.
- CLI behavior for multi-file source loading. Implemented for direct CLI inputs.
- Golden fixtures that include secondary source files. Implemented.

Remaining future work:

- Package or module search paths beyond explicit relative/absolute source paths.
- Re-export syntax for forwarding declarations from one module through another.
- Separate compilation or module artifacts instead of always recursively loading source and compiling one combined program.

Recommended split:

- Phase 14E: re-export syntax for forwarding declarations from one module
  through another, for example after deciding between `export name from
  "path";` and `export { name } from "path";`.
- Phase 14F: package/module search paths for less verbose imports. This should
  come after re-export rules because it affects source resolution and
  diagnostics more broadly.
- Phase 14G: separate compilation or module artifacts. Treat this as a larger
  compiler/backend design effort, not a small language polish task.

Why late: modules affect diagnostics, CLI source management, test layout, and name resolution across compilation units.

## Phase 15: Language Polish and Diagnostics

Goal: improve ergonomics after the core language grows.

Status: in progress. Phase 15A is implemented: located front-end diagnostics print the relevant source line and a caret while keeping one-line golden compatibility for broad fixtures. Phase 15B is implemented: anonymous function expressions beginning with `fun (` can appear directly as expression statements while named `fun name(...)` declarations keep their existing behavior. Phase 15C is implemented: imported-file and direct multi-file lexer, parser, and type diagnostics report source file paths with file-local snippets, while stdin and single-file pathless diagnostics remain supported and locationless diagnostics remain one-line. Phase 15D is implemented for numeric variable compound assignment (`+=`, `-=`, `*=`, `/=`). Index and field compound assignment remain future work.

Suggested features:

- Source snippets and carets for front-end diagnostics. Implemented, with file-aware paths for imported files and direct multi-file inputs.
- More parse recovery and multi-error reporting.
- Clear handling for lambda expression statements that begin with `fun`. Implemented by parser disambiguation between `fun name` declarations and `fun (` expressions.
- Compound assignment operators. Phase 15D is implemented for numeric variable
  targets only (`name += expr`, `name -= expr`, `name *= expr`, `name /= expr`).
  A later slice can extend compound assignment to `array[index]` and
  `object.field` targets after assignment target reuse is designed.
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

Start with **Phase 14E: module re-export/search paths** if the priority is module ergonomics, or **Phase 15E: index/field compound assignment** if the priority is mutation ergonomics.
