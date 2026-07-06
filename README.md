# Compiler Design

Compiler Design is a small experimental programming language and compiler
project. The repository contains the language implementation, a C++17 compiler
pipeline, a C++ IR interpreter, bytecode artifact emission, and a standalone
Rust bytecode VM.

The language currently supports variables, lexical blocks, `if`/`else`,
`while`, `break`, `continue`, functions, closures, arrays, indexing, array
element assignment, structs, field access and assignment, short-circuit logical
operators, typed `let` declarations, typed function parameters and returns,
and builtins such as `len`, `push`, `pop`, `floor`, `ceil`, and `sqrt`.

The compiler pipeline includes:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- Type checker: resolves lexical scopes and checks implemented static type
  annotations.
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
- IR interpreter: executes that virtual-register IR directly.
- Bytecode compiler: lowers register IR into a bytecode program and `.cdbc` artifacts for the Rust VM.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
let name: type = expression;
struct Name { field: type, ... }
print expression;
if expression { declaration* } [else { declaration* }]
while expression { declaration* }
break;
continue;
fun name(parameter[: type]*) [: type] { declaration* }
return [expression];
{ declaration* }
expression;
```

Type annotations support `number`, `bool`, `string`, `nil`, named struct types, and function types such as `fun(number): string`. Function type annotations may be used on `let` bindings, function parameters, and function returns. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`; expressions whose static type is still unknown remain flexible. Known function signatures are checked for assignment compatibility, call argument types, and function returns. Array element types, generic types, and nullable type syntax are not implemented yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.

`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. `break;` exits the nearest enclosing `while`, and `continue;` skips to that loop's next condition check. Loop-control statements outside loops are type errors; nested function bodies cannot break or continue an enclosing loop.

Functions are values. Named functions use `fun name(parameter[: type]*) [: type] { declaration* }`, and anonymous function expressions use `fun (parameter[: type]*) [: type] { declaration* }`. Known function values carry arity, parameter types when annotated, and inferred or annotated return types for static checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported, though recursive return inference remains conservative. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Example function type annotations: `let f: fun(number): number = fun (x: number): number { return x + 1; };` and `fun apply(f: fun(number): number, x: number): number { return f(x); }`.

Struct literals use `{ field: expression, ... }`, preserve field order when printed, and support field reads with `value.field`. Existing fields can be reassigned with `value.field = expression`; the assignment evaluates to the assigned value. Structs are reference values with identity equality, so aliases observe field mutation. Assigning a missing field is a runtime error.

Named struct declarations define static field shapes:

```cd
struct Person { name: string, age: number }
let p: Person = { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Named structs are static-only in this phase: runtime values remain anonymous struct values. Literal initialization of a named struct requires an exact field match, field order does not matter, and field access/assignment on known named struct values is statically checked. Constructor syntax such as `Person { ... }`, methods, recursive struct types, and runtime type names are not implemented yet.

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;      // 37
print person.age = 38; // 38
```

The builtin `len(value)` returns a number for arrays and strings. `len([1, 2, 3])` returns `3`, and `len("hello")` returns `5` using the current runtime string byte length. Statically known non-array and non-string arguments are type errors; unknown arguments are checked at runtime. A user binding named `len` shadows the builtin in its lexical scope.

The native stdlib functions `push(array, value)` and `pop(array)` mutate arrays in place. `push` appends a value and returns `nil`; `pop` removes and returns the last value. Arrays are reference values, so aliases observe length changes. Calling `pop([])` is a runtime error. User bindings named `push` or `pop` shadow the stdlib functions, matching `len` shadowing behavior.

The numeric native stdlib functions `floor(number)`, `ceil(number)`, and `sqrt(number)` each return a number. `sqrt` rejects negative inputs at runtime. User bindings with the same names shadow these stdlib functions.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Arrays: `[element, ...]` and `[]`; elements may be mixed runtime types.
- Structs: `{ name: value, ... }`, field reads `value.name`, and existing-field assignment `value.name = expression`.
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Calls: `callee(argument*)`
- Indexing: `array[index]` reads an element. Indexes must be integer numbers in range.
- Array element assignment: `array[index] = value` mutates an existing element and evaluates to the assigned value. Arrays are reference values, so aliases observe element and length mutation through `push(array, value)` and `pop(array)`.
- Struct field assignment: `object.field = value` mutates an existing field and evaluates to the assigned value. Structs are reference values, so aliases observe field mutation. Assigning a missing field is a runtime error.
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

Golden CLI tests live under `tests/golden`. Add a new directory with `input.cd` and expected `ast.out`, `ir.out`, `bytecode.out`, or `run.out` files to cover successful syntax and backend behavior. Runtime-error fixtures live in `tests/golden/runtime_errors`: for `example.cd`, add matching `example.run.err` and `example.exit` files. Parse-error fixtures live in `tests/golden/parse_errors`: for `example.cd`, add matching `example.err` and `example.exit` files. Type-error fixtures live in `tests/golden/type_errors`: for `example.cd`, add matching `example.err` and `example.exit` files.

To refresh golden files after an intentional output change:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

## Run

```sh
./build/compiler_design examples/hello.cd
./build/compiler_design --tokens examples/hello.cd
./build/compiler_design --ir examples/hello.cd
./build/compiler_design --run examples/hello.cd
./build/compiler_design --bytecode examples/hello.cd
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
```

Multiple input files may be provided. They are read in command-line order and compiled as one combined program:

```sh
./build/compiler_design --run lib.cd main.cd
./build/compiler_design --emit-bytecode program.cdbc lib.cd main.cd
```

`--run` executes the C++ IR interpreter. `--bytecode` remains a debug-print mode for inspecting compiler output. Bytecode execution is handled by the Rust VM via `.cdbc` artifacts:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

If no file is provided, source is read from stdin. Diagnostics currently report line and column in the combined source rather than original file names.
