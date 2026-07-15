# Agent Development Notes

This file is project memory for Codex/AI agents working in this repository. Read it before making compiler, grammar, IR, test, or documentation changes.

## Agent Workflow Defaults

- Do not invoke or assume `superpowers` skills by default.
- Use the normal repository workflow unless the user explicitly requests a
  particular skill or workflow.
- Existing files under `docs/superpowers/` are historical project plans and
  specifications; their presence does not require activating the skill system.

## Project Overview

This is a small C++17 Compiler Design front-end/interpreter project. It currently has:

- A lexer that turns source text into tokens.
- A recursive-descent parser that builds an AST.
- AST printing in a compact prefix/tree format.
- An IR compiler that lowers AST nodes to a small virtual-register, three-address IR.
- A CLI binary named `compiler_design`.
- Python golden tests that verify AST, IR, bytecode, run, parse-error, and runtime-error outputs.

## Architecture Map

- `include/Token.hpp`, `src/Lexer.cpp`: token definitions and lexical scanning.
- `include/Parser.hpp`, `src/Parser.cpp`: grammar and recursive-descent parsing.
- `include/Ast.hpp`, `src/Ast.cpp`: AST node types and AST printer output.
- `include/IR.hpp`, `src/IR.cpp`: IR opcodes, instructions, constants, names, registers, and IR printer output.
- `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: AST-to-IR lowering.
- `include/Bytecode.hpp`, `src/Bytecode.cpp`: bytecode opcodes, program/function containers, and bytecode printer output.
- `include/BytecodeTextEmitter.hpp`, `src/BytecodeTextEmitter.cpp`: stable `.cdbc` text artifact emission for the Rust VM boundary.
- `include/BytecodeCompiler.hpp`, `src/BytecodeCompiler.cpp`: IR-to-bytecode lowering.
- `vm-rs/src/vm.rs`, `vm-rs/src/value.rs`, `vm-rs/src/runtime.rs`: Rust `.cdbc` bytecode execution, runtime values, shared cells, closures, arrays, and structs.
- `include/Value.hpp`, `src/Value.cpp`: runtime value representation and formatting.
- `src/main.cpp`: CLI modes and top-level error handling.
- `docs/language-grammar.ebnf`: implemented grammar and precedence reference.
- `tests/run_golden_tests.py`: golden test runner.
- `tests/run_golden_tests_selftest.py`: unit tests for golden runner behavior.
- `tests/golden/`: CLI fixtures for successful programs, parse errors, and runtime errors.

## Build and Verification Commands

Use these commands from the repository root:

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

Run the relevant subset during development. Before claiming work is complete, run the full set above and report the exact commands and results.

To refresh golden files after an intentional output change:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

`--update` rewrites only expected files that already exist. Use `--case <substring>` to limit refreshes to specific fixtures, and add `--update-missing` only when you intentionally want to create missing success outputs such as `ast.out`, `ir.out`, or `bytecode.out`. Review refreshed goldens before committing them.

## Language Extension Workflow

When adding syntax or changing grammar, update all layers that are affected:

1. Add or adjust tokens in `include/Token.hpp` and `src/Lexer.cpp` when the syntax needs new lexical forms.
2. Update AST node definitions and printing in `include/Ast.hpp` and `src/Ast.cpp` when the syntax creates new tree shapes.
3. Update parser declarations in `include/Parser.hpp` and parsing logic in `src/Parser.cpp`.
4. Preserve precedence by placing parser methods in the correct recursive-descent level.
5. Add parse-error golden coverage for invalid syntax when applicable.
6. Update `docs/language-grammar.ebnf` so the grammar matches the parser.
7. Update `README.md` if user-visible language behavior changes.

Prefer small, testable changes. For new language features, add failing fixtures or selftests first when practical, then implement the minimal compiler changes, then refresh expected outputs intentionally.

## Backend / IR Extension Workflow

When syntax affects runtime behavior or code generation:

1. Add new `IROp` values in `include/IR.hpp` only when existing IR operations cannot express the behavior cleanly.
2. Update IR construction and printing in `src/IR.cpp`.
3. Lower AST nodes in `src/IRCompiler.cpp`.
4. Lower IR to bytecode in `src/BytecodeCompiler.cpp` and execute runtime behavior in the Rust VM under `vm-rs/src/`.
5. Add success golden cases with `ir.out`, `bytecode.out`, and/or `run.out` as appropriate.
6. Add runtime-error fixtures for invalid runtime behavior.
7. Keep parse errors, compile errors, and runtime errors distinct.

Use `StoreVar`-style operations for declarations/initialization and assignment-specific operations for updating existing bindings when the distinction matters.

When IR behavior changes, update bytecode lowering and the Rust VM path unless the change is intentionally IR-only. Bytecode lowering should preserve current IR semantics, and Rust VM execution should match `run.out` fixtures covered by `tests/run_rust_vm_tests.py`.

When changing bytecode opcodes or artifact formatting, update `docs/bytecode-text-format.md`, the C++ `BytecodeTextEmitter`, Rust parser/formatter in `vm-rs/src/format.rs`, and `tests/bytecode_artifacts/` together.

When changing Rust VM execution semantics, update `vm-rs/src/vm.rs`, focused Rust unit tests, and `tests/run_rust_vm_tests.py` coverage together.

## Golden Test Conventions

Successful program fixtures live in their own directories:

```text
tests/golden/<case>/input.cd
tests/golden/<case>/ast.out
tests/golden/<case>/ir.out
tests/golden/<case>/run.out
```

A successful fixture may include one or more expected output files. In non-update mode, a success fixture with no expected files is a test failure. Successful fixtures may include `bytecode.out` for `--bytecode`. Bytecode execution parity is covered by `tests/run_rust_vm_tests.py`, not by C++ bytecode-run golden files.

Runtime-error fixtures live under `tests/golden/runtime_errors`:

```text
tests/golden/runtime_errors/<case>.cd
tests/golden/runtime_errors/<case>.run.err
tests/golden/runtime_errors/<case>.exit
```

Parse-error fixtures live under `tests/golden/parse_errors`:

```text
tests/golden/parse_errors/<case>.cd
tests/golden/parse_errors/<case>.err
tests/golden/parse_errors/<case>.exit
```

Type-error fixtures live under `tests/golden/type_errors`:

```text
tests/golden/type_errors/<case>.cd
tests/golden/type_errors/<case>.err
tests/golden/type_errors/<case>.exit
```


Import-error fixtures live under `tests/golden/import_errors`:

```text
tests/golden/import_errors/<case>.cd
tests/golden/import_errors/<case>.err
tests/golden/import_errors/<case>.exit
```


Parse/type error `.err` files may either contain the first diagnostic line only or the full snippet form. Use the full snippet form for fixtures that intentionally cover caret placement.

Runtime-error, parse-error, type-error, and import-error fixtures should not produce stdout. The runner checks stderr and exit code.

## Diagnostic Output Convention

Language diagnostics use this stable shape:

```text
<Kind> error at <line>:<column>: <message>
  <source line>
  <caret>
<Kind> error: <message>
```

For file-backed lexer, parser, and type diagnostics in imported files and direct multi-file inputs, the first line may include a file path: `<Kind> error at <path>:<line>:<column>: <message>`. For stdin and single-file pathless diagnostics, the first line remains `<Kind> error at <line>:<column>: <message>`. The CLI appends the relevant source line and caret for located diagnostics. Compile, import loading, and runtime diagnostics are currently locationless unless explicitly changed by a future slice. After intentional diagnostic format changes, refresh and review parse/type/runtime/import error goldens. Lexer errors do not yet have a dedicated golden fixture category.

## Documentation Update Rules

- Update `docs/language-grammar.ebnf` whenever the implemented parser grammar or precedence changes.
- Update `README.md` whenever user-visible language features, CLI behavior, or test workflows change.
- Keep docs concise and aligned with actual implementation. Do not document planned behavior as if it exists.
- For substantial features, keep design and implementation plans under `docs/superpowers/specs/` and `docs/superpowers/plans/`.

## Git and Workspace Hygiene

- Check `git status --short` before editing and before finishing.
- Do not commit generated artifacts such as `tests/__pycache__/` or build outputs.
- Remove `tests/__pycache__/` after running Python tests.
- Keep commits focused: tests, implementation, docs, and golden updates should be easy to review.
- Do not overwrite unrelated user changes.

## Current Language Semantics and Limitations

- Supported statements include `let`, `print`, `if`/`else`, `while`, C-style `for`, array/range `for-in`, `break`, `continue`, `fun`, `return`, top-level `import`, blocks, statement-level `match`, and expression statements.
- Supported expressions include literals, arrays, maps, ranges, structs, indexing, index-assignment validation, field access, field assignment, variables, calls, function expressions, exhaustive enum `match` expressions, grouping, unary operators, binary/logical operators, assignment expressions, and numeric compound assignment for variables, array elements, and struct fields.
- Enum match statements use block arms; match expressions use comma-separated `pattern => expression` arms with an optional trailing comma. Both forms require exhaustive coverage or a wildcard/binding arm, support nested positional variant patterns, and keep pattern bindings local to each arm. Match expression arm results must have compatible types.
- `let name: type = expression;`, function parameter annotations, and function return annotations check explicit type names for `number`, `bool`, `string`, and `nil`. Function type annotations use `fun(type, ...): type` and may appear in `let`, parameter, and return annotations. Named functions, named-struct methods, and anonymous function expressions may declare inferred type parameters such as `fun identity<T>(value: T): T`, `fun echo<T>(value: T): T`, and `fun<T>(value: T): T`; calls recursively infer concrete arguments through existing array, nullable, and function shapes, and callers may provide all explicit type arguments such as `identity<number>(42)`, `alias<string>("hello")`, and `box.echo<number>(42)`; unannotated aliases retain generic signatures. Generic function values are not coerced to monomorphic function annotations. The built-in `map<K, V>` form is implemented; constraints and other generic container syntax are not implemented. Nullable annotations use postfix `?`, such as `number?`, `Person?`, `[number?]`, and `[number]?`. `nil` is assignable to `T?`, `T` is assignable to `T?`, and `T?` is not assignable to non-nullable `T` except inside direct `if` nil-check branches on simple variables (`if (x != nil) { ... }` and the `else` branch of `if (x == nil) { ... } else { ... }`). Nullable narrowing does not yet apply to fields, array elements, logical compositions, loops, or post-branch flow. Known function signatures are checked for assignment compatibility, call argument types, and function returns. Unannotated `let` bindings infer known initializer types, while unannotated function parameters, unannotated function returns, and array element types are not fully inferred yet.
- Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, and same-scope duplicate declarations are type errors.
- Assignment has the form `name = expression`, is right-associative, updates the nearest resolved binding, and evaluates to the assigned value.
- Numeric compound assignment supports `name += expression`, `array[index] += expression`, and `object.field += expression`, plus `-=`, `*=`, and `/=`. It updates an existing target, evaluates to the assigned value, and is numeric-only for both the old target value and right-hand value.
- `break;` exits the nearest enclosing `while`, C-style `for`, or array/range `for-in`; `continue;` skips to the next condition check of the nearest enclosing `while`, runs the increment before the next condition check of the nearest C-style `for`, or advances to the next item of the nearest `for-in`. Both are type errors outside loops, including inside nested function bodies that are lexically inside a loop. `for item in array { ... }` iterates arrays with a body-scoped item binding and snapshots the array length before iteration. `for item in range(...) { ... }` iterates immutable finite integer ranges. Strings, maps, and custom iterators are not implemented.
- Reading or assigning an undefined variable is a type error before IR compilation.
- Runtime values currently include nil, numbers, booleans, strings, functions, arrays, maps, ranges, and structs.
- A C++ bytecode backend lowers register IR to bytecode and `.cdbc` artifacts; Rust `compiler-design-vm` is the bytecode execution backend.
- Future VM backend work targets the Rust `compiler-design-vm` project under `vm-rs/` and `.cdbc` artifacts.
- The CLI accepts multiple input files for normal modes and `--emit-bytecode`; files are read in command-line order and compiled as one combined source. If no input file is provided, source is read from stdin except `--emit-bytecode`, which requires at least one file. Imported-file and direct multi-file front-end diagnostics report original file paths with file-local line/column; stdin and single-file pathless diagnostics remain pathless.
- Top-level `import "path";` directives load dependency files relative to the importing file or from CLI `-I` / `--import-path` search paths. Direct imports expose exported names in the importing module's top-level scope, while `import "path" as alias;` exposes them through qualified `alias.name` access. Imported files have module-private top-level scope; standalone `export name[, name...];` lists expose selected already-defined top-level declarations to importers, while private declarations remain visible only inside the imported file. Re-export declarations such as `export value, Point from "./lib.cd";` forward selected exports, including exported struct method metadata, without making those names local to the forwarding module. Duplicate canonical imports are no-ops, imports from stdin are rejected, and this phase has no package manifests, import maps, or separate compilation.
- Arrays are mutable runtime values with mixed element types. Indexing validates array-ness, numeric integer indexes, and bounds at runtime when static types are unknown; `array[index] = value` mutates an existing element and evaluates to the assigned value. The native stdlib includes shadowable `push(array, value)` and `pop(array)`. `push` mutates arrays in place and returns nil; `pop` mutates arrays in place and returns the removed value, with runtime error on empty arrays. The non-mutating array helpers `contains`, `slice`, `copy`, `concat`, and callback-based `map`, `filter`, and `reduce` are available in shadowable function form and unshadowed member-call sugar. `slice`, `copy`, `concat`, and `map` return fresh top-level arrays with shallow element results; `contains` uses existing runtime equality. `map` snapshots source elements and invokes a one-argument callback from left to right, preserving known callback return types as result element types. `filter` snapshots source elements and invokes a one-argument boolean predicate from left to right, retaining the original elements whose predicates return true. `reduce` requires an explicit initial accumulator, snapshots source elements, and invokes a two-argument `(accumulator, element)` callback from left to right, returning the final accumulator. Builtin member-call sugar also supports `array.push(value)`, `array.pop()`, `array.len()`, `array.map(callback)`, `array.filter(predicate)`, and `array.reduce(initial, callback)`. New stdlib functions should prefer the generic `native_call` path rather than bespoke opcodes; `len` remains a legacy dedicated opcode for now.
- Maps are ordered shared reference values with identity equality. Map literals use `{ key: value }` and `map<K, V>` annotations; keys are limited to `nil`, `number`, `bool`, and `string`. `map[key]` reads an existing entry and raises `map key not found` when absent; `map[key] = value` inserts or updates and evaluates to the assigned value. `len` and `contains` accept maps, and `map.len()`/`map.contains(key)` are unshadowed member-call forms. Map deletion, map `for-in`, custom iterators, and map compound assignment are not implemented.
- Ranges are immutable finite integer sequences constructed by shadowable `range(stop)`, `range(start, stop)`, or `range(start, stop, step)`. They are half-open, require finite integer bounds and a non-zero integer step, support `range[index]`, `len`, `contains`, `range.contains(value)`, and `for-in`, and compare structurally by their start/stop/step values. Ranges print as `range(start, stop, step)` and report `range` from `typeOf`.
- The numeric native stdlib includes shadowable `floor(number)`, `ceil(number)`, and `sqrt(number)` helpers. They return numbers; `sqrt` raises a runtime error for negative inputs. New stdlib functions should prefer the generic `native_call` path rather than bespoke opcodes; `len` remains a legacy dedicated opcode for now.
- The string native stdlib includes shadowable `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` helpers. `str` uses the same formatting as `print`; `substr` and `charAt` use Unicode scalar-value offsets consistent with `len(string)` and validate integer bounds at runtime when needed. Builtin member-call sugar also supports `string.len()`, `string.substr(start, length)`, and `string.charAt(index)`.
- The debug native stdlib includes shadowable `typeOf(value)`, which returns runtime type names such as `"nil"`, `"number"`, `"bool"`, `"string"`, `"function"`, `"array"`, `"map"`, `"range"`, or named struct names such as `"Person"` and `"geo.Point"`.
- Source programs construct struct values only through declared named constructors such as `Name { field: value }` and `alias.Name { field: value }`; anonymous source struct literals such as `{ field: value }` are not supported. Field access `value.name` reads an existing field; statically known non-struct field access is a type error, dynamic non-struct or missing-field access is a runtime error. Field assignment `value.name = expression` mutates an existing field, evaluates to the assigned value, and aliases observe the mutation. Statically known non-struct field assignment is a type error; dynamic non-struct targets or missing fields are runtime errors. Named struct declarations `struct Name { field: type, ... }` define static field shapes. Named struct type annotations check exact struct constructor initialization, and known named struct field access/assignment is statically checked. Named constructor values carry runtime type-name metadata for `typeOf`; creating fields by assignment and recursive struct types are not implemented yet. Named structs may define methods in top-level `impl Name { fun method(...) ... }` blocks. Method calls `receiver.method(args...)` are statically resolved for known named struct receiver types and lower to ordinary function calls with the receiver passed as implicit `this`. Methods on exported/imported named structs are available through direct imports, namespace imports, and re-exports. Dynamic dispatch, inheritance, overloading, static methods, and function-valued field calls are not implemented yet.
- The builtin `len(value)` returns array/map/range element counts or Unicode scalar-value counts for strings as a number. User bindings named `len` shadow the function-style builtin; unknown argument types are checked at runtime. Builtin member-call sugar names (`push`, `pop`, `len`, `contains`, `slice`, `copy`, `concat`, `map`, `filter`, `reduce`, `substr`, `charAt`) are not shadowed by lexical bindings with the same names.
- Functions compile to an IR function table. Named functions and anonymous function expressions produce function values. Anonymous function expressions may appear directly as expression statements, for example `fun () { return nil; };`. Parameters and returns may be annotated with `number`, `bool`, `string`, `nil`, or function types such as `fun(number): string`; named functions and anonymous function expressions may add type parameters with `fun name<T>(...)` or `fun<T>(...)`, and call-site inference is erased before IR generation. Known function values carry arity, annotated parameter types, and conservative or annotated return types for static checks. Nested functions and function expressions are closures capturing enclosing locals by reference through shared runtime cells.
- Runtime variable environments store cells rather than raw values. Assignment mutates an existing cell so closures sharing that cell observe updates.

## Roadmap Hints

The active roadmap in `docs/roadmap.md` is now language-focused. The near-term sequence is remaining type-system extensions, collection extensions and higher-order APIs, then pattern-matching extensions. Broader iteration is not part of the active plan. Bytecode VM follow-ups such as GC, task scheduling, and JIT exploration are deferred backend tracks; start them only from a dedicated backend design spec and implementation plan. When adding language features, prefer vertical slices that update parser, AST, type checker, IR, bytecode, interpreters/VM, docs, and goldens together.
