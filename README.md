# Compiler Demo

A small C++17 compiler front-end demo. It currently implements:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
- IR interpreter: executes that virtual-register IR directly.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
let name: type = expression;
print expression;
if expression { declaration* } [else { declaration* }]
{ declaration* }
expression;
```

Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope: variables declared inside a block are not visible outside it, and inner blocks may shadow outer variables. Re-declaring a variable in the same scope is a type error.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Grouping: `(expression)`
- Unary operators: `!`, `-`
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, or `run.out` files to cover successful syntax. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files. Type-error fixtures live in `tests/golden/type_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.

To refresh golden files after an intentional output change:

```sh
python3 tests/run_golden_tests.py ./build/compiler_demo --update
```

## Run

```sh
./build/compiler_demo examples/hello.cd
./build/compiler_demo --tokens examples/hello.cd
./build/compiler_demo --ir examples/hello.cd
./build/compiler_demo --run examples/hello.cd
```

If no file is provided, source is read from stdin.
