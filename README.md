# Compiler Demo

A small C++17 compiler front-end demo. It currently implements:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
- IR interpreter: executes that virtual-register IR directly.
- Bytecode compiler: lowers register IR into a bytecode program.
- Bytecode VM: executes the bytecode backend in parallel with the IR interpreter.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
let name: type = expression;
print expression;
if expression { declaration* } [else { declaration* }]
while expression { declaration* }
fun name(parameter*) { declaration* }
return [expression];
{ declaration* }
expression;
```

Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, and `array`; expressions whose static type is still unknown, such as function call results, remain flexible. Function parameters, function returns, and array element types are not fully inferred yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.

`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break` and `continue` are not implemented yet.

Functions are values. Named functions use `fun name(parameter*) { declaration* }`, and anonymous function expressions use `fun (parameter*) { declaration* }`. Known function values carry arity and inferred return types for static checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported, though recursive return inference remains conservative. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Function parameter types, return type annotations, and function type annotations are not implemented yet.

The builtin `len(value)` returns a number for arrays and strings. `len([1, 2, 3])` returns `3`, and `len("hello")` returns `5` using the current runtime string byte length. Statically known non-array and non-string arguments are type errors; unknown arguments are checked at runtime. A user binding named `len` shadows the builtin in its lexical scope.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Arrays: `[element, ...]` and `[]`; elements may be mixed runtime types.
- Function expressions: `fun (parameter*) { declaration* }`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Calls: `callee(argument*)`
- Indexing: `array[index]` reads an element. Indexes must be integer numbers in range. Array mutation and `push` are not implemented yet.
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

Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, `bytecode.out`, `run.out`, or `run_bytecode.out` files to cover successful syntax and backend behavior. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files, and optionally `example.run_bytecode.err` and `example.run_bytecode.exit` files for bytecode VM diagnostics. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files. Type-error fixtures live in `tests/golden/type_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.

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
./build/compiler_demo --bytecode examples/hello.cd
./build/compiler_demo --run-bytecode examples/hello.cd
```

`--run` executes the existing IR interpreter. `--run-bytecode` executes the newer bytecode VM. They are expected to match for implemented language features.

If no file is provided, source is read from stdin.
