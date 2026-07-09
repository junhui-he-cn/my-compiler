# Member Call Sugar Design

## Goal

Add a small method-style call syntax for existing collection and string helpers, without introducing a general method system.

This is Phase 12E from the language roadmap. The first slice should improve ergonomics for APIs that already exist as builtins while preserving current runtime semantics and bytecode/Rust VM parity.

## User-Facing Semantics

Member-call sugar rewrites selected calls by passing the receiver as the first argument to an existing builtin/helper.

Supported forms:

| Member syntax | Equivalent existing call |
| --- | --- |
| `array.push(value)` | `push(array, value)` |
| `array.pop()` | `pop(array)` |
| `array.len()` | `len(array)` |
| `string.len()` | `len(string)` |
| `string.substr(start, length)` | `substr(string, start, length)` |
| `string.charAt(index)` | `charAt(string, index)` |

Examples:

```cd
let xs = [1];
xs.push(2);
print xs.len(); // 2
print xs.pop(); // 2

let text = "hello";
print text.len();          // 5
print text.substr(1, 3);   // ell
print text.charAt(4);      // o
```

`push` still mutates arrays in place and returns `nil`. `pop` still mutates arrays in place and returns the removed value. String helpers keep current byte-offset behavior.

## Scope

Included:

- Parse `receiver.name(args...)` as a distinct member-call expression when a field access is immediately followed by call parentheses.
- Support only the member names `push`, `pop`, `len`, `substr`, and `charAt`.
- Type-check supported member calls using the same rules as the corresponding existing function-style helpers, with the receiver treated as argument 1.
- Lower supported member calls to existing IR operations:
  - `receiver.len()` lowers to existing `len` IR.
  - other supported member calls lower to existing `native_call` names with the receiver prepended to the argument list.
- Preserve C++ IR interpreter, bytecode compiler, bytecode text, and Rust VM semantics by reusing existing opcodes and native-call dispatch.
- Add success, type-error, runtime-error, bytecode artifact, Rust VM parity, and docs coverage.

Excluded:

- User-defined methods.
- Struct method declarations.
- A `this` keyword or receiver binding.
- Calling function-valued fields as bound methods.
- Optional chaining such as `value?.method()`.
- General property lookup changes.
- New bytecode opcodes.

## Shadowing and Name Resolution

Function-style helpers remain shadowable exactly as today:

```cd
let push = fun (array, value) { print "custom"; };
push([], 1); // calls user function
```

Member-call sugar is not shadowed by user variables named `push`, `pop`, `len`, `substr`, or `charAt`:

```cd
let push = fun (array, value) { print "custom"; };
let xs = [];
xs.push(1); // uses builtin member sugar and mutates xs
```

Rationale: `receiver.push(...)` names an operation on the receiver, not a lexical variable lookup. This keeps member calls stable and avoids surprising interactions with local helper names.

## Parsing and AST

Current parsing represents `receiver.name` as `FieldAccessExpr`, then a following `(...)` as a normal `CallExpr` whose callee is that field access. This slice should add a distinct `MemberCallExpr` AST node so type checking and lowering can distinguish builtin member sugar from ordinary dynamic field calls.

Proposed AST shape:

```cpp
struct MemberCallExpr final : Expr {
    ExprPtr receiver;
    Token name;
    Token paren;
    std::vector<ExprPtr> arguments;
};
```

AST printer output should be stable and explicit, for example:

```text
(member-call receiver name arg1 arg2)
```

Parser approach:

- Keep `receiver.name` as field access when no call follows.
- When `finishCall` sees a callee that is `FieldAccessExpr`, convert it to `MemberCallExpr` instead of a normal `CallExpr`.
- This means `object.field()` is no longer parsed as “read field then call” in this first member-call slice. Because function-valued field calls were not a documented feature, this is acceptable for now.

## Static Type Checking

Type checking should recognize only these member names and arities:

- `push`: receiver + one value argument.
- `pop`: receiver + zero value arguments.
- `len`: receiver + zero value arguments.
- `substr`: receiver + start + length.
- `charAt`: receiver + index.

Receiver rules:

- `push` and `pop` require array receivers when the receiver type is statically known.
- `len` accepts known array or string receivers.
- `substr` and `charAt` require string receivers when known.
- Unknown receivers are accepted statically and checked at runtime through existing native helper behavior.

Argument and return rules:

- `push` checks known array element type against the pushed value and returns `nil`.
- `pop` returns the known array element type if available, otherwise unknown.
- `len` returns `number`.
- `substr` requires known numeric `start` and `length` arguments and returns `string`.
- `charAt` requires known numeric `index` and returns `string`.

Unsupported member names are type errors, not runtime fallback:

```cd
[1].map(1); // Type error: unknown member call `map`
```

Known wrong receivers are type errors:

```cd
1.push(2);       // push expects array receiver, got number
[1].substr(0,1); // substr expects string receiver, got array
```

## Lowering

Lowering should reuse existing IR:

- `receiver.len()`:
  - compile receiver once
  - emit existing `Len`
- `receiver.push(value)`:
  - compile receiver once
  - compile `value`
  - emit `native_call "push"` with `[receiver, value]`
- `receiver.pop()`:
  - emit `native_call "pop"` with `[receiver]`
- `receiver.substr(start, length)`:
  - emit `native_call "substr"` with `[receiver, start, length]`
- `receiver.charAt(index)`:
  - emit `native_call "charAt"` with `[receiver, index]`

Receiver evaluation must happen exactly once and before the explicit arguments, matching ordinary call evaluation order.

No bytecode or Rust VM changes should be needed beyond adding artifact/parity tests, because emitted IR uses existing operations.

## Runtime Behavior

Dynamic receiver failures should reuse existing runtime messages from the underlying helpers where possible:

- `id(123).push(1)` -> `Runtime error: push expects array as first argument`
- `id(123).len()` -> `Runtime error: len expects array or string`
- `id(123).substr(0, 1)` -> `Runtime error: substr expects string as first argument`

This keeps function-style and method-style runtime behavior aligned.

## Tests

Add success fixtures for:

- Array member calls: `push`, `pop`, and `len`.
- String member calls: `len`, `substr`, and `charAt`.
- Shadowing behavior where a local `push` does not affect `xs.push(value)`.
- Expression-result behavior for `pop`, `len`, `substr`, and `charAt`.

Add type-error fixtures for:

- Wrong receiver type for `push`.
- Wrong receiver type for `substr`.
- Wrong argument type for `charAt`.
- Wrong arity for `push` or `len`.
- Unknown member call name.

Add runtime-error fixtures for dynamic unknown receivers:

- `id(123).push(1)`.
- `id(123).len()`.
- `id(123).substr(0, 1)`.

Add bytecode artifact and Rust VM parity coverage for at least one array and one string member call fixture.

## Documentation

Update:

- `README.md`: document supported member-call sugar and the fact that user-defined methods are not implemented.
- `docs/language-grammar.ebnf`: mention member-call syntax as part of call suffixes.
- `docs/roadmap.md`: mark Phase 12E implemented for builtin member-call sugar and keep full user-defined methods as future work.
- `AGENTS.md`: update current language semantics and limitations.
