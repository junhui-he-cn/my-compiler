# Compiler Design

Compiler Design is a small experimental programming language and compiler
project. The repository contains the language implementation, a C++17 compiler
pipeline, bytecode artifact emission, and a standalone Rust bytecode VM.

The language currently supports variables, lexical blocks, `if`/`else`,
`while`, `break`, `continue`, functions, closures, arrays, indexing, array
element assignment, numeric compound assignment for variables, array elements, and struct fields, structs, field access and assignment, short-circuit logical
operators, typed `let` declarations, typed function parameters and returns,
source imports, and builtins such as `len`, `push`, `pop`, `floor`, `ceil`,
`sqrt`, `str`, `substr`, `charAt`, and `typeOf`.

The compiler pipeline includes:

- Lexer: turns source text into tokens.
- Parser: builds a simple AST from tokens.
- Type checker: resolves lexical scopes and checks implemented static type
  annotations.
- IR compiler: lowers the AST to a small three-address intermediate representation with virtual registers.
- Rust VM: executes emitted `.cdbc` bytecode artifacts via `compiler-design-vm`.
- Bytecode compiler: lowers register IR into a bytecode program and `.cdbc` artifacts for the Rust VM.
- AST printer: prints the parsed program in prefix form.

## Language

Supported statements:

```text
let name = expression;
let name: type = expression;
import "path" [as alias];
export name[, name...];
struct Name { field: type, ... }
print expression;
if expression { declaration* } [else { declaration* }]
while expression { declaration* }
for [initializer]; [condition]; [increment] { declaration* }
for item in array { declaration* }
break;
continue;
fun name(parameter[: type]*) [: type] { declaration* }
return [expression];
{ declaration* }
expression;
```

Type annotations support `number`, `bool`, `string`, `nil`, named struct types, namespaced struct types such as `math.Point`, array types such as `[number]` and `[[string]]`, and function types such as `fun(number): string`. Function type annotations may be used on `let` bindings, function parameters, and function returns. Array type annotations may be used in the same positions and carry static element types through indexing, index assignment, `push`, and `pop`. Nullable annotations use postfix `?`, for example `number?`, `Person?`, `[number?]`, and `[number]?`. A value of type `T?` may be either `nil` or a `T`; `nil` is not assignable to non-nullable `T`, and `T?` is not assignable back to `T` except inside supported simple-variable nil-check branches: direct checks such as `if (x != nil) { ... }`, the `else` branch of `if (x == nil) { ... } else { ... }`, conservative `&&` guards such as `if (x != nil && y != nil) { ... }`, and conservative `||` else branches such as `if (x == nil || y == nil) { ... } else { ... }`; supported logical guards can narrow multiple simple variables. Function parameter and return annotations may use nullable types, such as `fun(number?): string?`. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and anonymous `struct`; non-empty unannotated array literals infer an element type only when all known elements have the same type. Mixed unannotated arrays and empty unannotated arrays remain dynamic arrays with unknown element type. Known function signatures are checked for assignment compatibility, call argument types, and function returns. Generic types and broader flow-sensitive nullable narrowing for fields, indexes, loops, and post-branch flow are not implemented yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.

Examples of nullable annotations:

```cd
let age: number? = nil;
age = 42;
age = nil;
let values: [number?] = [1, nil, 3];
let maybeValues: [number]? = nil;
```

`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. C-style `for` loops use clauses: `for initializer; condition; increment { ... }`. Each clause is optional. A `let` initializer is scoped to the loop condition, body, and increment, and is not visible after the loop. `for item in array { ... }` iterates arrays in index order. The item binding is scoped to the loop body and may shadow an outer variable. The loop snapshots the array length before iteration, so appending during iteration does not extend the loop; existing elements are still read at their current index when reached. `break;` exits the nearest enclosing loop. `continue;` in a `while` skips to that loop's next condition check, in a C-style `for` evaluates the increment before checking the condition again, and in a `for-in` advances to the next item. Loop-control statements outside loops are type errors; nested function bodies cannot break or continue an enclosing loop.

### Source imports

A top-level source file can load another source file with:

```cd
import "./lib.cd";
```

Import paths are resolved relative to the file that contains the import.
Imported files have module-private top-level scope. Only declarations marked
with `export` are introduced into the importing file's top-level scope:

```cd
// lib.cd
let hidden = 1;
fun visible() { return hidden + 1; }
export visible;

// main.cd
import "./lib.cd";
print visible();
```

Using `import "path" as alias;` keeps exported names out of the importing
file's top-level scope. Values and functions are accessed as `alias.name`, and
exported struct types may be used as `alias.Type` in annotations and
constructors such as `alias.Type { field: value }`:

```cd
// main.cd
import "./lib.cd" as lib;
print lib.visible();
```

Importing the same canonical file more than once is a no-op, which allows
shared helper files to be imported through multiple paths in the source graph.
This phase supports standalone export lists such as `export value;` and
`export value, helper, Point;` for already-defined top-level variables,
functions, and structs. It does not add re-export syntax, package search paths,
separate compilation, or imports from stdin. `import` inside strings or `//`
comments is ignored by the loader.

Functions are values. Named functions use `fun name(parameter[: type]*) [: type] { declaration* }`, and anonymous function expressions use `fun (parameter[: type]*) [: type] { declaration* }`. Anonymous function expressions may appear in expression positions, including direct expression statements such as `fun () { return nil; };`. Known function values carry arity, parameter types when annotated, and inferred or annotated return types for static checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported, though recursive return inference remains conservative. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Example function type annotations: `let f: fun(number): number = fun (x: number): number { return x + 1; };` and `fun apply(f: fun(number): number, x: number): number { return f(x); }`.

Struct literals use `{ field: expression, ... }`, preserve field order when printed, and support field reads with `value.field`. Existing fields can be reassigned with `value.field = expression`; the assignment evaluates to the assigned value. Structs are reference values with identity equality, so aliases observe field mutation. Assigning a missing field is a runtime error.

Named struct declarations define static field shapes:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Named structs are static-only in this phase: runtime values remain anonymous struct values. Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, and allow fields in any order. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)`, recursive struct types, and runtime type names are not implemented yet.

Local named structs may define first-slice methods in top-level `impl` blocks. Methods are statically resolved on known named struct receiver types, and method calls lower to ordinary function calls with the receiver passed as implicit `this`:

```cd
struct Person { name: string }
impl Person {
  fun greet(): string { return this.name; }
}
let p: Person = Person { name: "Ada" };
print p.greet();
```

Inside a method, `this` has the impl struct type; field assignment through `this.field = value` mutates the receiver. Methods on anonymous or imported/namespaced structs, method export/import behavior, inheritance, overloading, dynamic dispatch, static methods, and function-valued field calls are not implemented yet.

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;      // 37
print person.age = 38; // 38
```

The builtin `len(value)` returns a number for arrays and strings. `len([1, 2, 3])` returns `3`, and `len("hello")` returns `5` using the current runtime string byte length. Statically known non-array and non-string arguments are type errors; unknown arguments are checked at runtime. A user binding named `len` shadows the builtin in its lexical scope.

The native stdlib functions `push(array, value)` and `pop(array)` mutate arrays in place. `push` appends a value and returns `nil`; `pop` removes and returns the last value. When an array has a known element type, `push` statically checks the appended value and `pop` returns that element type. Arrays are reference values, so aliases observe length changes. Calling `pop([])` is a runtime error. User bindings named `push` or `pop` shadow the stdlib functions, matching `len` shadowing behavior.

The numeric native stdlib functions `floor(number)`, `ceil(number)`, and `sqrt(number)` each return a number. `sqrt` rejects negative inputs at runtime. User bindings with the same names shadow these stdlib functions.

The string native stdlib includes `str(value)`, `substr(string, start, length)`, and `charAt(string, index)`. `str` returns the same textual representation used by `print`. `substr` and `charAt` use byte offsets, matching the current `len(string)` byte-length behavior; offsets must be finite integer numbers and in bounds. User bindings with the same names shadow these builtins.

Builtin member-call sugar is available for selected array and string helpers: `array.push(value)`, `array.pop()`, `array.len()`, `string.len()`, `string.substr(start, length)`, and `string.charAt(index)`. These forms lower to the existing builtins with the receiver as the first argument; lexical bindings named `push`, `pop`, `len`, `substr`, or `charAt` do not shadow member-call sugar.

The debug native stdlib function `typeOf(value)` returns the current runtime type name as a string: `"nil"`, `"number"`, `"bool"`, `"string"`, `"function"`, `"array"`, or `"struct"`. Named struct values return `"struct"`, and arrays return `"array"` regardless of static element type. A user binding named `typeOf` shadows the builtin.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Arrays: `[element, ...]` and `[]`; elements may be mixed runtime types.
- Structs: `{ name: value, ... }`, field reads `value.name`, and existing-field assignment `value.name = expression`.
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`, including direct expression statements such as `fun () { return nil; };`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Compound assignment: `name += expression`, `array[index] += expression`, and `object.field += expression` forms, plus `-=`, `*=`, and `/=`, update the target and evaluate to the assigned value. Compound assignment is numeric-only for both the old target value and the right-hand value.
- Calls: `callee(argument*)` and member calls `receiver.method(argument*)`
- Indexing: `array[index]` reads an element. Indexes must be integer numbers in range.
- Array element assignment: `array[index] = value` mutates an existing element and evaluates to the assigned value. Arrays are reference values, so aliases observe element and length mutation through `push(array, value)` and `pop(array)`.
- Struct field assignment: `object.field = value` mutates an existing field and evaluates to the assigned value. Structs are reference values, so aliases observe field mutation. Assigning a missing field is a runtime error.
- Logical operators: `left || right` and `left && right` short-circuit using the same truthiness rules as `if` and `!`. They return the selected operand value rather than forcing a boolean.
- Grouping: `(expression)`
- Unary operators: `!`, `-`
- Binary operators: `*`, `/`, `+`, `-`, `<`, `<=`, `>`, `>=`, `==`, `!=`

## Diagnostics

Compiler errors are reported as `Lex`, `Parse`, `Type`, `Compile`, `Import`, or `Runtime` errors. Front-end diagnostics include a `line:column` location when available, for example:

```text
Parse error at 1:14: expected expression
  print add(1, );
               ^
Type error at 1:7: undefined variable `missing`
  print missing;
        ^
```

Located lexer, parser, and type diagnostics include a source line and caret. For imported files and direct multi-file inputs, diagnostics report the original file path plus file-local line and column. For stdin and single-file inputs without imports, diagnostics remain pathless. Locationless diagnostics, including import loading, compile, and runtime errors, remain one-line messages.

## Build

```sh
cmake -S . -B build
cmake --build build
```

To enable compiler warnings for local C++ builds:

```sh
cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON
cmake --build build
```

To enable AddressSanitizer and UndefinedBehaviorSanitizer for local C++ builds:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
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

By default, `--update` rewrites only expected files that already exist, so a success fixture with only `run.out` will not accidentally create `ast.out`, `ir.out`, or `bytecode.out`. Use `--update-missing` when you intentionally want to create missing success outputs, and use `--case <substring>` to refresh or run only matching fixtures:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update --case my_fixture
python3 tests/run_golden_tests.py ./build/compiler_design --update --update-missing --case my_fixture
```

## Run

```sh
./build/compiler_design examples/hello.cd
./build/compiler_design --tokens examples/hello.cd
./build/compiler_design --ir examples/hello.cd
./build/compiler_design --bytecode examples/hello.cd
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
```

Multiple input files may be provided. They are read in command-line order and compiled as one combined program:

```sh
./build/compiler_design --emit-bytecode program.cdbc lib.cd main.cd
```

`--bytecode` remains a debug-print mode for inspecting compiler output. Program execution is handled by the Rust VM via `.cdbc` artifacts:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

If no file is provided, source is read from stdin. Imported-file and direct multi-file front-end diagnostics report original file paths with file-local line and column; stdin diagnostics remain pathless.
