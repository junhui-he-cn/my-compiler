# Agent Development Notes

This file is project memory for Codex/AI agents working in this repository. Read it before making compiler, grammar, IR, test, or documentation changes.

## Project Overview

This is a small C++17 compiler front-end/interpreter demo. It currently has:

- A lexer that turns source text into tokens.
- A recursive-descent parser that builds an AST.
- AST printing in a compact prefix/tree format.
- An IR compiler that lowers AST nodes to a small virtual-register, three-address IR.
- An IR interpreter that executes the register IR.
- A CLI binary named `compiler_demo`.
- Python golden tests that verify AST, IR, run output, parse errors, and runtime errors.

## Architecture Map

- `include/Token.hpp`, `src/Lexer.cpp`: token definitions and lexical scanning.
- `include/Parser.hpp`, `src/Parser.cpp`: grammar and recursive-descent parsing.
- `include/Ast.hpp`, `src/Ast.cpp`: AST node types and AST printer output.
- `include/IR.hpp`, `src/IR.cpp`: IR opcodes, instructions, constants, names, registers, and IR printer output.
- `include/IRCompiler.hpp`, `src/IRCompiler.cpp`: AST-to-IR lowering.
- `include/IRInterpreter.hpp`, `src/IRInterpreter.cpp`: runtime execution of IR and runtime error behavior.
- `include/Bytecode.hpp`, `src/Bytecode.cpp`: bytecode opcodes, program/function containers, and bytecode printer output.
- `include/BytecodeCompiler.hpp`, `src/BytecodeCompiler.cpp`: IR-to-bytecode lowering.
- `include/BytecodeVM.hpp`, `src/BytecodeVM.cpp`: bytecode VM execution, VM heap boundary, and VM thread/frame state.
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
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Run the relevant subset during development. Before claiming work is complete, run the full set above and report the exact commands and results.

To refresh golden files after an intentional output change:

```sh
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

Review refreshed goldens before committing them.

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
4. Execute new IR behavior in `src/IRInterpreter.cpp`.
5. Add success golden cases with `ir.out` and/or `run.out` as appropriate.
6. Add runtime-error fixtures for invalid runtime behavior.
7. Keep parse errors, compile errors, and runtime errors distinct.

Use `StoreVar`-style operations for declarations/initialization and assignment-specific operations for updating existing bindings when the distinction matters.

When IR behavior changes, update both the IR interpreter and the bytecode backend unless the change is intentionally IR-only. Bytecode lowering should preserve current IR semantics, and `--run-bytecode` should match `--run` for supported programs.

## Golden Test Conventions

Successful program fixtures live in their own directories:

```text
tests/golden/<case>/input.cd
tests/golden/<case>/ast.out
tests/golden/<case>/ir.out
tests/golden/<case>/run.out
```

A successful fixture may include one or more expected output files. In non-update mode, a success fixture with no expected files is a test failure. Successful fixtures may include `bytecode.out` for `--bytecode` and `run_bytecode.out` for `--run-bytecode`.

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

Runtime-error fixtures may include `.run_bytecode.err` and `.run_bytecode.exit` to check bytecode VM runtime diagnostics.

Runtime-error, parse-error, and type-error fixtures should not produce stdout. The runner checks stderr and exit code.

## Diagnostic Output Convention

Language diagnostics use this stable shape:

```text
<Kind> error at <line>:<column>: <message>
<Kind> error: <message>
```

Use locations for lexer, parser, and type errors when a source token/location is available. Compile and runtime diagnostics are currently locationless. After intentional diagnostic format changes, refresh and review parse/type/runtime error goldens. Lexer errors do not yet have a dedicated golden fixture category.

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

- Supported statements include `let`, `print`, `if`/`else`, `while`, `fun`, `return`, blocks, and expression statements.
- Supported expressions include literals, arrays, indexing, variables, calls, grouping, unary operators, binary/logical operators, and assignment expressions.
- `let name: type = expression;` checks explicit annotations for `number`, `bool`, `string`, and `nil`; unannotated variables are accepted without full inference.
- Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, and same-scope duplicate declarations are type errors.
- Assignment has the form `name = expression`, is right-associative, updates the nearest resolved binding, and evaluates to the assigned value.
- Reading or assigning an undefined variable is a type error before IR compilation.
- Runtime values currently include nil, numbers, booleans, strings, functions, and arrays.
- A parallel bytecode backend lowers register IR to bytecode; `--run-bytecode` should match `--run` for supported programs.
- Arrays are immutable-length runtime values with mixed element types. Indexing is read-only and validates array-ness, numeric integer indexes, and bounds at runtime when static types are unknown.
- Functions compile to an IR function table. Nested `fun` declarations are closures and capture enclosing local variables by reference through shared runtime cells.
- Runtime variable environments store cells rather than raw values. Assignment mutates an existing cell so closures sharing that cell observe updates.

## Roadmap Hints

Likely future work includes richer statements such as loops/functions, more operators, fuller type inference/checking, and a more complete backend. When adding these, prefer vertical slices that update parser, AST, IR, interpreter, docs, and goldens together.
