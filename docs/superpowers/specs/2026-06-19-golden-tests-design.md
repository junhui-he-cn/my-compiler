# Golden CLI Tests Design

Date: 2026-06-19

## Goal

Replace the single Bash smoke test with a small CLI golden test system that is easy to extend while developing new syntax. The test system should verify end-to-end compiler behavior through the existing `compiler_demo` executable and remain runnable through CTest.

The standard verification command should stay:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current State

The project currently has one test script:

```text
tests/smoke_register_ir.sh
```

CTest invokes that script as `smoke_register_ir`. It checks a few `--run` results and a few register IR output patterns. This is useful as a smoke test, but it is not convenient for language development because adding a new syntax case means editing script logic instead of adding a focused test fixture.

## Target Architecture

Introduce a Python golden test runner:

```text
tests/run_golden_tests.py
```

Introduce a directory of test cases:

```text
tests/golden/
```

CTest should invoke the Python runner:

```cmake
add_test(
    NAME golden
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_demo>
)
```

The existing smoke test coverage should move into golden cases. The old `smoke_register_ir` CTest entry should be removed so there is only one test entry point to maintain.

## Successful Case Layout

A successful golden case is one directory with an `input.cd` file and one or more expected output files:

```text
tests/golden/arithmetic/
  input.cd
  ast.out
  ir.out
  run.out
```

The runner interprets the expected files as follows:

```text
ast.out -> compiler input.cd
ir.out  -> compiler --ir input.cd
run.out -> compiler --run input.cd
```

If an expected file is missing, that mode is skipped for that case. This keeps cases flexible: a future test can focus only on runtime output or only on IR output.

For the initial suite, each successful case should include all three files: `ast.out`, `ir.out`, and `run.out`.

## Runtime Error Case Layout

Runtime error cases live under a grouped directory:

```text
tests/golden/runtime_errors/
  undefined_variable.cd
  undefined_variable.run.err
  undefined_variable.exit
  division_by_zero.cd
  division_by_zero.run.err
  division_by_zero.exit
```

For each `*.cd` file in `runtime_errors`, the runner executes:

```sh
compiler --run case.cd
```

It compares:

```text
case.run.err -> stderr
case.exit    -> process exit code
```

Runtime error tests do not require AST or IR golden files in the initial design. They focus on preserving diagnostics and failure behavior for the interpreter path.

## Runner Behavior

`tests/run_golden_tests.py` should:

1. Accept the compiler executable path as its first argument.
2. Locate `tests/golden` relative to the script file.
3. Discover successful cases by finding directories under `tests/golden` that contain `input.cd`.
4. Discover runtime error cases by finding `*.cd` files under `tests/golden/runtime_errors`.
5. Run the requested compiler mode for each expected output file.
6. Compare actual output to golden files exactly.
7. Print a clear failure message and unified diff when output differs.
8. Exit with code `0` only if all discovered checks pass.

Example failure output:

```text
FAIL arithmetic --run stdout mismatch

--- expected
+++ actual
@@
-7
+8
```

The runner should report a summary such as:

```text
golden tests: 24 passed, 0 failed
```

## Update Mode

The runner should support an explicit update mode:

```sh
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

In update mode, the runner rewrites expected output files from the current compiler output instead of failing on mismatches. This is intended for intentional formatting changes, such as changing AST or IR printer output.

CTest must not use `--update`.

## Initial Successful Cases

The first test suite should cover the current language baseline.

### `tests/golden/literals/input.cd`

```text
print nil;
print true;
print false;
print 123;
print "hello";
```

### `tests/golden/arithmetic/input.cd`

```text
print 1 + 2 * 3;
print (1 + 2) * 3;
print -5 + 10;
print 8 / 2;
```

### `tests/golden/comparison/input.cd`

```text
print 1 < 2;
print 2 <= 2;
print 3 > 4;
print 4 >= 4;
print 1 == 1;
print 1 != 2;
print "a" == "a";
print nil == nil;
```

### `tests/golden/strings/input.cd`

```text
print "a" + "b";
print "hello";
print "hello" == "hello";
print "hello" != "world";
```

### `tests/golden/truthiness/input.cd`

```text
print !nil;
print !false;
print !true;
print !0;
print !"";
```

### `tests/golden/variables/input.cd`

```text
let answer = 40 + 2;
let ok = answer == 42;
print answer;
print ok;
```

### `tests/golden/expression_statement/input.cd`

```text
let x = 10;
x + 1;
print x;
```

Each successful case should include:

```text
ast.out
ir.out
run.out
```

## Initial Runtime Error Cases

### `tests/golden/runtime_errors/undefined_variable.cd`

```text
print missing;
```

Expected behavior:

```text
undefined_variable.exit -> 1
undefined_variable.run.err -> IR runtime error: undefined variable `missing`
```

### `tests/golden/runtime_errors/division_by_zero.cd`

```text
print 1 / 0;
```

Expected behavior:

```text
division_by_zero.exit -> 1
division_by_zero.run.err -> IR runtime error: division by zero
```

### `tests/golden/runtime_errors/invalid_add.cd`

```text
print "a" + 1;
```

Expected behavior:

```text
invalid_add.exit -> 1
invalid_add.run.err -> IR runtime error: add expects two numbers or two strings
```

Error golden files should include the trailing newline printed by the executable.

## CMake Integration

Replace the current smoke test registration with the golden test registration:

```cmake
enable_testing()
add_test(
    NAME golden
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/run_golden_tests.py $<TARGET_FILE:compiler_demo>
)
```

The old `tests/smoke_register_ir.sh` can be deleted after its coverage is represented by golden cases.

## README Updates

The README should gain a short testing section:

    ## Test

    ```sh
    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure
    ```

    Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover new syntax.

    To refresh golden files after an intentional output change:

    ```sh
    python3 tests/run_golden_tests.py ./build/compiler_demo --update
    ```

## Developer Workflow for New Syntax

When adding a new syntax feature:

1. Add a new golden case under `tests/golden/<feature>/input.cd`.
2. Add or generate expected `ast.out`, `ir.out`, and `run.out` files.
3. Confirm the golden output represents the intended behavior.
4. Implement the language change.
5. Run:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

This keeps each language feature tied to an end-to-end regression case.

## Non-Goals

This work does not add:

- GoogleTest, Catch2, or another C++ unit test framework.
- Direct C++ unit tests for Lexer, Parser, IRCompiler, or IRInterpreter.
- Token golden tests.
- Parser recovery tests.
- Fuzz tests or property tests.

Those can be added later if the project needs finer-grained diagnostics. The immediate goal is a low-friction end-to-end test system for language development.
