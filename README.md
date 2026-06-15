# Compiler Demo

A small C++17 compiler front-end demo. It currently implements:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
print expression;
expression;
```

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Variables: `name`
- Grouping: `(expression)`
- Unary operators: `!`, `-`
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
./build/compiler_demo examples/hello.cd
./build/compiler_demo --tokens examples/hello.cd
```

If no file is provided, source is read from stdin.

