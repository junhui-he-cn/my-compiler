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
while expression { declaration* }
{ declaration* }
expression;
```

Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.

`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break` and `continue` are not implemented yet.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Logical operators: `left || right` and `left && right` short-circuit using the same truthiness rules as `if` and `!`. They return the selected operand value rather than forcing a boolean.
- Grouping: `(expression)`
- Unary operators: `!`, `-`
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`

## Diagnostics

Compiler errors are reported as `Lex`, `Parse`, `Type`, `Compile`, or `Runtime` errors. Front-end diagnostics include a `line:column` location when available, for example:

```text
Parse error at 1:15: expected expression
Type error at 1:7: undefined variable `missing`
```

Runtime diagnostics currently do not include source locations.

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
