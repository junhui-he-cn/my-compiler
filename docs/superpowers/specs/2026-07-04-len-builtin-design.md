# `len` Builtin Design

## Goal

Add a small collection/string length builtin as Phase 10A:

```cd
print len([1, 2, 3]);
print len("hello");
```

`len` returns a `number` and works for arrays and strings. The feature should behave identically in the IR interpreter path (`--run`) and bytecode VM path (`--run-bytecode`).

## Scope

This phase implements only `len(value)`.

It does not implement:

- a general builtin registry;
- array mutation;
- `push` or `pop`;
- string slicing;
- Unicode grapheme counting;
- new parser syntax;
- new AST node shapes.

## User-Visible Semantics

`len` is called using ordinary call syntax:

```cd
len(expression)
```

Supported runtime inputs:

- array values: returns the element count;
- string values: returns `std::string::size()` for the current runtime string storage.

Examples:

```cd
print len([]);          // 0
print len([1, 2, 3]);   // 3
print len("hello");    // 5
```

The return value is a number:

```cd
let n = len([true, "x", nil]);
print n + 1; // 4
```

## Name Resolution and Shadowing

`len` behaves like an implicit builtin only when no user binding named `len` is visible.

Example:

```cd
print len([1, 2]); // builtin len

let len = 123;
print len;         // user variable
print len([1]);    // type error: can only call functions
```

This preserves lexical scope behavior and avoids making `len` a keyword.

Implementation rule:

- when checking or compiling a call whose callee is a `VariableExpr` named `len`, first ask the resolved lexical environment whether a user binding exists;
- if a user binding exists, handle the call normally;
- if no user binding exists, handle it as the builtin.

## Static Type Checking

The type checker recognizes builtin `len` calls before normal undefined-variable handling for unbound `len` callees.

Rules for `len(argument)`:

1. Exactly one argument is required.
2. If the argument type is statically known:
   - `array` is accepted;
   - `string` is accepted;
   - `number`, `bool`, `nil`, and `function` are rejected with a type error.
3. If the argument type is `Unknown`, the call is accepted and runtime checks decide.
4. The static result type is `number`.
5. Builtin calls have no function arity or return metadata because the call result is a number value, not a function value.

Suggested diagnostics:

```text
Type error at <line>:<column>: expected 1 arguments but got 0
Type error at <line>:<column>: expected 1 arguments but got 2
Type error at <line>:<column>: len expects array or string, got number
```

The location should use the call's closing parenthesis token, matching existing call arity diagnostics.

## Runtime Behavior

At runtime `len` checks the actual value type.

Accepted values:

- arrays: return element count as a numeric value;
- strings: return byte length as a numeric value.

Rejected values:

- nil;
- numbers;
- booleans;
- functions.

Runtime diagnostic:

```text
Runtime error: len expects array or string
```

Runtime checking is required because statically unknown expressions may still reach `len`:

```cd
fun id(x) {
  return x;
}

print len(id(123)); // type checker allows; runtime rejects
```

## IR Design

Add a new IR operation:

```cpp
IROp::Len
```

Shape:

- destination register: length result;
- first source register: value to measure;
- no second source register;
- no name payload.

Printed IR should follow existing unary-op style. A representative output is:

```text
r1 = len r0
```

Lowering:

- `IRCompiler` detects unshadowed `len(...)` calls and emits `Len` instead of normal `Call`.
- The argument expression is compiled normally.
- The result register receives the numeric length.

Normal function calls and shadowed `len` calls continue to compile through existing call machinery.

## IR Interpreter Design

`IRInterpreter` executes `IROp::Len` by reading the source value and producing a number value.

Behavior:

- array: use array element count;
- string: use string byte length;
- otherwise throw `RuntimeError("len expects array or string")`.

This should use existing `Value` APIs and formatting conventions.

## Bytecode Design

Add a bytecode opcode:

```cpp
OpCode::Len
```

Lowering:

- `BytecodeCompiler` lowers `IROp::Len` to `OpCode::Len`.

VM behavior:

- pop or read the operand according to the current bytecode instruction format;
- compute the length for arrays/strings;
- store the numeric result in the destination;
- throw the same runtime diagnostic as the IR interpreter for unsupported values.

`--run` and `--run-bytecode` must produce matching output or errors.

## Parser and AST Impact

No parser or AST changes are needed.

`len([1, 2])` remains a normal `CallExpr` with callee `VariableExpr("len")`. Builtin handling lives in type checking and lowering.

`docs/language-grammar.ebnf` does not need to change because no grammar production changes.

## Testing Strategy

Add success golden coverage with both interpreter paths:

```cd
print len([]);
print len([1, 2, 3]);
print len("hello");
let n = len([true, "x", nil]);
print n + 1;
```

Expected output:

```text
0
3
5
4
```

Add shadowing success coverage:

```cd
let len = 123;
print len;
```

Expected output:

```text
123
```

Add type-error coverage:

```cd
print len();
print len([1], [2]);
print len(123);
let n = len([1]);
n = "bad";
```

Use separate fixtures so each expected diagnostic is stable.

Add runtime-error coverage for statically unknown invalid input:

```cd
fun id(x) {
  return x;
}
print len(id(123));
```

Expected runtime diagnostic:

```text
Runtime error: len expects array or string
```

## Documentation Updates

Update:

- `README.md` with `len(array|string)` behavior and shadowing note;
- `docs/roadmap.md` to mark Phase 10A implemented once code lands;
- `AGENTS.md` current semantics with builtin `len` behavior.

Do not update `docs/language-grammar.ebnf` because the syntax is unchanged.


## Non-Goals

This phase does not implement:

- a general builtin registry or standard library table;
- array mutation or collection update helpers;
- user-defined native functions;
- Unicode-aware string length;
- any new syntax or grammar changes;
- changes to function value representation.

## Risks and Mitigations

Risk: treating `len` as a builtin could accidentally override user variables.

Mitigation: only use builtin behavior when no lexical binding named `len` exists.

Risk: IR and bytecode implementations could diverge.

Mitigation: add `run.out` and `run_bytecode.out` for every success fixture and use matching runtime-error tests when supported by the golden runner.

Risk: string length semantics could be mistaken for Unicode character count.

Mitigation: document that this phase uses current runtime string byte length.

Risk: adding a one-off builtin path could make future builtins repetitive.

Mitigation: keep this deliberately small for Phase 10A; revisit a general builtin registry when adding multiple standard builtins in Phase 13.
