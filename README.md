# Compiler Design

Compiler Design is a small experimental programming language and compiler
project. The repository contains the language implementation, a C++17 compiler
pipeline, bytecode artifact emission, and a standalone Rust bytecode VM.

The language currently supports variables, lexical blocks, `if`/`else`,
`while`, `break`, `continue`, functions, closures, arrays, maps, ranges, enums,
and exhaustive pattern matching, indexing, array
and map element assignment, numeric compound assignment for variables, array elements, and struct fields, structs, field access and assignment, short-circuit logical
operators, typed `let` declarations, typed function parameters and returns,
source imports, and builtins such as `len`, `push`, `pop`, `floor`, `ceil`,
`sqrt`, `str`, `substr`, `charAt`, `contains`, `slice`, `copy`, `concat`, and
`typeOf`.

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
enum Name { Variant(type, ...), Other }
match expression { Pattern => { declaration* } }
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

Type annotations support `number`, `bool`, `string`, `nil`, named struct types, namespaced struct types such as `math.Point`, array types such as `[number]` and `[[string]]`, and function types such as `fun(number): string`. Function type annotations may be used on `let` bindings, function parameters, and function returns. Array type annotations may be used in the same positions and carry static element types through indexing, index assignment, `push`, and `pop`. Nullable annotations use postfix `?`, for example `number?`, `Person?`, `[number?]`, and `[number]?`. A value of type `T?` may be either `nil` or a `T`; `nil` is not assignable to non-nullable `T`, and `T?` is not assignable back to `T` except inside supported simple-variable nil-check branches: direct checks such as `if (x != nil) { ... }`, the `else` branch of `if (x == nil) { ... } else { ... }`, conservative `&&` guards such as `if (x != nil && y != nil) { ... }`, and conservative `||` else branches such as `if (x == nil || y == nil) { ... } else { ... }`; supported logical guards can narrow multiple simple variables. Function parameter and return annotations may use nullable types, such as `fun(number?): string?`. Named functions may declare inferred type parameters, for example `fun identity<T>(value: T): T { return value; }`; calls infer `T` from known arguments, callers may provide all explicit type arguments such as `identity<number>(42)`, aliases retain generic signatures, and existing array annotations such as `[T]` can carry the inferred type. Generic methods and lambdas, constraints, and generic container syntax beyond the built-in `map<K, V>` form are not implemented. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, `array`, and named struct constructors; non-empty unannotated array literals merge compatible element types, including nullable combinations such as `[1, nil]` becoming `[number?]` and nested homogeneous arrays preserving nested element types. Empty unannotated arrays start as dynamic arrays with unknown element type, then direct simple-variable mutations such as `push(xs, value)`, `xs.push(value)`, and `xs[index] = value` can refine the element type for later indexing, `pop`, and `for-in` reads. Mixed unannotated arrays and incompatible direct mutations fall back to dynamic arrays with unknown element type, while explicit array annotations remain strict. Known function signatures are checked for assignment compatibility, call argument types, and function returns; anonymous function expressions in expected function-typed positions use that context for unannotated parameter and return checking. Broader flow-sensitive nullable narrowing for fields, indexes, loops, and post-branch flow is not implemented. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.

The built-in `map<K, V>` type is the first implemented generic collection form.
Map literals use `{ key: value }` in expression position, infer key and value
types independently, and fall back to a dynamic map when either component is
unknown or incompatible. Map keys are limited to `nil`, `number`, `bool`, and
`string`; arrays, structs, functions, and maps are rejected as keys. Maps keep
insertion order, use identity equality, and are shared reference values. When
a literal repeats an equal key, the last value wins while the key keeps its
original insertion position.

Examples of nullable annotations:

```cd
let age: number? = nil;
age = 42;
age = nil;
let values: [number?] = [1, nil, 3];
let maybeValues: [number]? = nil;
```

`while` evaluates its condition before each iteration, uses the same truthiness rules as `if`, `!`, `&&`, and `||`, and requires a block body. C-style `for` loops use clauses: `for initializer; condition; increment { ... }`. Each clause is optional. A `let` initializer is scoped to the loop condition, body, and increment, and is not visible after the loop. `for item in array { ... }` iterates arrays in index order, and `for item in range(...) { ... }` iterates finite integer ranges. The item binding is scoped to the loop body and may shadow an outer variable. Array iteration snapshots the array length before iteration, while ranges are immutable. `break;` exits the nearest enclosing loop. `continue;` in a `while` skips to that loop's next condition check, in a C-style `for` evaluates the increment before checking the condition again, and in a `for-in` advances to the next item. Loop-control statements outside loops are type errors; nested function bodies cannot break or continue an enclosing loop.

### Source imports

A top-level source file can load another source file with:

```cd
import "./lib.cd";
```

Import paths are resolved relative to the file that contains the import. The
CLI can also add search directories for non-explicit import strings:

```sh
compiler_design -I stdlib --import-path vendor main.cd
```

For `import "math";`, the loader first tries `math` and then `math.cd` next to
the importing file. If neither exists, it tries the same raw and `.cd` candidates
under each `-I` or `--import-path` directory in command-line order. Paths that
start with `./`, `../`, or an absolute root are explicit paths; they use the
importing file's directory and the `.cd` fallback, but they do not fall back to
CLI search paths. Re-export source clauses use the same resolution rules.

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
exported struct types may be used as `alias.Type` in annotations, constructors
such as `alias.Type { field: value }`, and method calls on `alias.Type` values:

```cd
// main.cd
import "./lib.cd" as lib;
print lib.visible();
```

Importing the same canonical file more than once is a no-op, which allows
shared helper files to be imported through multiple paths in the source graph.
Standalone export lists such as `export value;` and `export value, helper,
Point;` expose already-defined top-level variables, functions, and structs.
Re-export declarations such as `export value, Point from "./lib.cd";` forward
selected exports from another source file without making those names local to
the forwarding module; import the dependency explicitly when the forwarding
module also needs to use the name. Re-exported structs forward their method
metadata for direct and namespace importers. The module system does not add
renaming re-exports, wildcard exports, package manifests, import maps, separate
compilation, or imports from stdin. `import` inside strings or `//` comments is
ignored by the loader.

Exported enums are available through direct imports and namespace aliases,
including qualified annotations, constructors, and patterns such as
lib.Outcome.Good(value).

Functions are values. Named functions use `fun name[<T, U>](parameter[: type]*) [: type] { declaration* }`, and anonymous function expressions use `fun (parameter[: type]*) [: type] { declaration* }`. Generic named functions infer type parameters at each call, including direct and namespace imports; callers may also provide all type arguments explicitly, such as `identity<number>(42)` or `lib.identity<string>("hello")`; an unannotated alias preserves the generic signature. Anonymous function expressions and methods are not generic in this slice. Anonymous function expressions may appear in expression positions, including direct expression statements such as `fun () { return nil; };`. Known function values carry arity, parameter types when annotated or contextually typed, and inferred, annotated, or contextually checked return types for static checks, including variables initialized from named functions or function expressions. `return expression;` returns a value, `return;` returns `nil`, and reaching the end of a function also returns `nil`. Recursive named calls are supported, though recursive return inference remains conservative. Nested functions and function expressions are by-reference closures: they capture enclosing local variables through shared runtime cells, so reads and assignments share the same variable even after the outer function returns. Example function type annotations: `let f: fun(number): number = fun (x: number): number { return x + 1; };` and `fun apply(f: fun(number): number, x: number): number { return f(x); }`.

Struct values are created with named constructor expressions such as `Person { name: "Ada", age: 36 }` after a matching `struct Person { ... }` declaration. Constructors preserve declared field behavior, require exact field names, and allow fields in any order. Field reads use `value.field`. Existing fields can be reassigned with `value.field = expression`; the assignment evaluates to the assigned value. Structs are reference values with identity equality, so aliases observe field mutation. Assigning a missing field is a runtime error when the target type is not statically known, and a type error when it is known.

Named struct declarations define static field shapes:

```cd
struct Person { name: string, age: number }
let p = Person { name: "Ada", age: 36 };
print p.name;
p.age = 37;
```

Named constructor expressions infer the named static type and attach the named runtime type used by `typeOf`; all structs keep the same field-only print format. Annotated bindings must still use an explicit constructor, for example `let p: Person = Person { name: "Ada", age: 36 };`. Field annotations may refer to non-recursive struct names declared later in the same scope, but recursive struct field types such as `struct Node { next: Node? }` are explicitly rejected for now. Field access/assignment on known named struct values is statically checked. Anonymous source struct literals are not a separate form: bare braces such as `{ name: "Ada" }` are map literals, while named constructors such as `Person { name: "Ada" }` remain structs. Constructor functions such as `Person(...)` are not implemented.

Enums define explicit alternatives with positional payloads. Recursive enum
references are allowed:

```cd
enum Result { Ok(number), Err(string), Empty }
let result = Result.Ok(7);

match result {
  Result.Ok(value) => { print value; }
  Result.Err(message) => { print message; }
  Result.Empty => { print 0; }
}
```

Enum constructors use `Enum.Variant(...)`; unit variants use an empty call.
Match is a statement with arm-local bindings and must cover every variant, or
use `_` or a binding pattern. Nested patterns are supported. Enum values use
structural equality and print as `Enum.Variant` or `Enum.Variant(value, ...)`.
`typeOf` reports the enum name. Match expressions, guards, named payload
fields, generic enums, and nullable enum patterns are not implemented.

Local named structs may define first-slice methods in top-level `impl` blocks.
Methods are statically resolved on known named struct receiver types, and
method calls lower to ordinary function calls with the receiver passed as
implicit `this`:

```cd
struct Person { name: string }
impl Person {
  fun greet(): string { return this.name; }
}
let p: Person = Person { name: "Ada" };
print p.greet();
```

Inside a method, `this` has the impl struct type; field assignment through `this.field = value` mutates the receiver. Methods on exported/imported named structs are available through direct imports and namespace imports, so `import "path";` supports `value.method()` and `import "path" as alias;` supports methods on `alias.Type` values. Inheritance, overloading, dynamic dispatch, static methods, and function-valued field calls are not implemented yet.

```cd
struct Person { name: string, age: number }
let person = Person { name: "Ada", age: 36 };
person.age = 37;
print person.age;      // 37
print person.age = 38; // 38
```

The builtin `len(value)` returns a number for arrays, maps, ranges, and strings.
`len([1, 2, 3])` returns `3`, `len({"a": 1})` returns `1`, and string length
counts Unicode scalar values: `len("hello")` is `5` and `len("你🙂")` is `2`.
Statically known values outside those four types are type errors; unknown
arguments are checked at runtime. A user binding named `len` shadows the
builtin in its lexical scope.

The native stdlib functions `push(array, value)` and `pop(array)` mutate arrays in place. `push` appends a value and returns `nil`; `pop` removes and returns the last value. When an array has a known element type, `push` statically checks the appended value and `pop` returns that element type. Arrays are reference values, so aliases observe length changes. Calling `pop([])` is a runtime error. User bindings named `push` or `pop` shadow the stdlib functions, matching `len` shadowing behavior.

The non-mutating array collection helpers are `contains(array, value)`,
`slice(array, start, length)`, `copy(array)`, and `concat(left, right)`.
`contains` uses the language's existing equality rules. `slice`, `copy`, and
`concat` allocate a new top-level array and shallow-copy elements, so nested
arrays, structs, and closures remain shared. Function-style names are shadowable;
the corresponding member forms `array.contains(value)`,
`array.slice(start, length)`, `array.copy()`, and `array.concat(right)` are
builtin member sugar and are not shadowed by lexical bindings.

Maps support `map[key]` lookup and `map[key] = value` upsert assignment. A
missing lookup is a runtime error (`map key not found`), while assigning an
existing key replaces its value and assigning a new key appends it. The
`contains` helper also accepts maps (`contains(map, key)` and
`map.contains(key)`) and tests key presence. `map.len()` is the member form of
`len(map)`. Equality between maps is identity-based, so aliases compare equal
but separately constructed maps do not. Map values print as
`map{key: value, ...}` in insertion order.
Map deletion, map `for-in`, custom iterators, and higher-order map helpers are
not implemented.

The native `range` helper constructs immutable finite integer ranges:
`range(stop)`, `range(start, stop)`, and `range(start, stop, step)`. Ranges are
half-open, so `range(1, 5)` contains `1, 2, 3, 4`; descending ranges require a
negative step, and a zero step is a runtime error. Bounds and steps must be
finite integers. Ranges support zero-based indexing, `len`, `contains`,
`for-in`, structural equality by `(start, stop, step)`, and print as
`range(start, stop, step)`.

The numeric native stdlib functions `floor(number)`, `ceil(number)`, and `sqrt(number)` each return a number. `sqrt` rejects negative inputs at runtime. User bindings with the same names shadow these stdlib functions.

The string native stdlib includes `str(value)`, `substr(string, start, length)`, and `charAt(string, index)`. `str` returns the same textual representation used by `print`. `substr` and `charAt` use Unicode scalar-value offsets and always return complete UTF-8 values: `substr("你🙂", 1, 1)` is `"🙂"`, and `charAt("你🙂", 1)` is `"🙂"`. Offsets must be finite integer numbers and in bounds. Combining marks are counted as separate scalar values; grapheme segmentation and normalization are not provided. User bindings with the same names shadow these builtins.

Builtin member-call sugar is available for selected array, map, and string
helpers: `array.push(value)`, `array.pop()`, `array.len()`,
`array.contains(value)`, `array.slice(start, length)`, `array.copy()`,
`array.concat(right)`, `map.len()`, `map.contains(key)`, `string.len()`,
`string.substr(start, length)`, `string.charAt(index)`, and
`range.contains(value)`. These forms lower
to the existing builtins with the receiver as the first argument; lexical
bindings named `push`, `pop`, `len`, `contains`, `slice`, `copy`, `concat`,
`substr`, or `charAt` do not shadow member-call sugar.

The debug native stdlib function `typeOf(value)` returns the current runtime type name as a string: primitive values report `"nil"`, `"number"`, `"bool"`, `"string"`, or `"function"`; arrays report `"array"`; maps report `"map"`; ranges report `"range"`; enum values report their enum name such as `"Result"`; named struct values report their runtime struct name such as `"Person"` or `"geo.Point"`. A user binding named `typeOf` shadows the builtin.

Supported expressions:

- Literals: numbers, strings, `true`, `false`, `nil`
- Arrays: `[element, ...]` and `[]`; elements may be mixed runtime types.
- Ranges: `range(stop)`, `range(start, stop)`, and `range(start, stop, step)`;
  ranges are finite, half-open, integer sequences.
- Maps: `{ key: value, ... }` and `{}`; keys are `nil`, `number`, `bool`, or
  `string`, and entries preserve insertion order.
- Structs: named constructors such as `Name { field: value, ... }`, field reads `value.name`, and existing-field assignment `value.name = expression`.
- Enums and patterns: `enum Name { Variant(type, ...) }`, qualified
  constructors such as `Name.Variant(value)`, and exhaustive statement-level
  `match` with wildcard, binding, and nested variant patterns.
- Function expressions: `fun (parameter[: type]*) [: type] { declaration* }`, including direct expression statements such as `fun () { return nil; };`
- Variables: `name`
- Assignment: `name = expression` updates an existing variable and evaluates to the assigned value. Use `let` to declare variables before assigning to them.
- Compound assignment: `name += expression`, `array[index] += expression`, and `object.field += expression` forms, plus `-=`, `*=`, and `/=`, update the target and evaluate to the assigned value. Compound assignment is numeric-only for both the old target value and the right-hand value.
- Calls: `callee(argument*)`, explicit generic calls such as `callee<number>(argument*)`, and member calls `receiver.method(argument*)` or `receiver.method<number>(argument*)`
- Indexing: `array[index]` reads an array element, `map[key]` reads a map entry,
  and `range[index]` reads a range element. Array and range indexes must be
  integer numbers in range. Missing map keys are runtime
  errors.
- Array element assignment: `array[index] = value` mutates an existing element and evaluates to the assigned value. Arrays are reference values, so aliases observe element and length mutation through `push(array, value)` and `pop(array)`.
- Map element assignment: `map[key] = value` inserts or updates an entry and
  evaluates to the assigned value. Maps are reference values, so aliases observe
  updates. Map compound assignment is not implemented.
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

Located lexer, parser, and type diagnostics include a source line and caret. For imported files and direct multi-file inputs, diagnostics report the original file path plus file-local line and column. For stdin and single-file inputs without imports, front-end diagnostics remain pathless. Runtime failures from compiler-emitted `.cdbc` artifacts include the embedded source path, source line, caret, and an innermost-to-outermost `Call stack`; metadata-free hand-written or legacy artifacts retain the one-line runtime form. When the parser can recover at statement boundaries, a single run may report multiple `Parse` diagnostics before later compiler phases are skipped. Lexer, import, type, and compile diagnostics still stop at the first reported error.

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
./build/compiler_design --module-interface examples/hello.cd
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
```

Multiple input files may be provided. They are read in command-line order and compiled as one combined program:

```sh
./build/compiler_design --emit-bytecode program.cdbc lib.cd main.cd
```

`--bytecode` remains a debug-print mode for inspecting compiler output. `--module-interface` prints the type-checked public API metadata for every loaded module, including exported values, named structs, struct fields, and exported struct method signatures. It is a debug/introspection mode only; it does not emit a separate-compilation artifact or run a linker. Program execution is handled by the Rust VM via `.cdbc` artifacts:

The interface output also reports exported enum variants and their payload types.

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

If no file is provided, source is read from stdin. Imported-file and direct multi-file front-end diagnostics report original file paths with file-local line and column; stdin diagnostics remain pathless.
