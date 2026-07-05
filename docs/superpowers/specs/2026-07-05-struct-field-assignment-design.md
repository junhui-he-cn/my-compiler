# Struct Field Assignment Design

## Goal

Add Phase 12B struct field assignment: `object.field = value` mutates an existing field on an anonymous struct value and evaluates to the assigned value.

This phase builds directly on Phase 12A anonymous struct literals and field access. It does not introduce named structs, methods, or static field-shape tracking.

## User-Visible Syntax

Field assignment uses an existing field access expression as an assignment target:

```cd
let person = { name: "Ada", age: 36 };
person.age = 37;
print person.age;
```

It is an expression and returns the assigned value:

```cd
print person.age = 38; // prints 38
```

Nested field assignment is valid:

```cd
let outer = { inner: { x: 1 } };
outer.inner.x = 2;
print outer.inner.x; // 2
```

Structs are reference values, so aliases observe mutation:

```cd
let a = { x: 1 };
let b = a;
b.x = 2;
print a.x; // 2
```

## Scope

This phase includes:

- Parsing `FieldAccessExpr` as an assignment target.
- AST representation and printing for field assignment.
- Static type checks for known non-struct assignment targets.
- Runtime mutation of existing struct fields.
- IR, bytecode, `.cdbc`, and Rust VM parity.
- Success, parse-error, type-error, and runtime-error golden coverage.
- Documentation updates.

This phase does not include:

- Creating new fields by assignment.
- Field deletion.
- Field assignment type tracking.
- Struct type annotations.
- Named struct declarations.
- Methods or member calls such as `xs.push(value)`.
- Compound assignment such as `person.age += 1`.

## Grammar

The expression grammar remains right-associative assignment:

```ebnf
assignment = logicalOr, [ "=", assignment ] ;
```

Extend valid parser assignment targets from variables and array indexes to include field access:

```ebnf
assignmentTarget = identifier
                 | call, "[", expression, "]"
                 | call, ".", identifier ;
```

The parser should still reject all other assignment targets as parse errors.

## AST Shape and Printing

Add:

```cpp
struct FieldAssignExpr final : Expr {
    FieldAssignExpr(ExprPtr object, Token name, ExprPtr value);
    void print(std::ostream& out) const override;

    ExprPtr object;
    Token name;
    ExprPtr value;
};
```

Parser construction should move the object expression out of the parsed `FieldAccessExpr`, like existing index assignment moves out of `IndexExpr`.

AST printer shape:

```text
Expr (= (field person age) 37)
Print (= (field person age) 38)
Expr (= (field (field outer inner) x) 2)
```

## Static Type Checking

Rules:

- Check the object expression.
- Check the assigned value expression.
- If the object type is known and not `struct`, raise a type error.
- If the object type is `unknown`, allow and defer validation to runtime.
- The assignment expression result type is the assigned value type.

Suggested diagnostic:

```text
Type error at 1:5: can only assign fields on structs
```

The diagnostic location should use the field name token when possible.

## Runtime Semantics

`object.field = value` mutates an existing field in-place and returns `value`.

If `object` is not a struct at runtime:

```text
Runtime error: can only assign fields on structs
```

If the field does not exist:

```text
Runtime error: undefined field `field`
```

Assignment does not create new fields in this phase. This keeps struct shape stable and avoids introducing dynamic shape-growth semantics before named structs and field typing are designed.

## IR and Bytecode

Add IR operation:

```text
vD = assign_field vObject.field, vValue
```

Use the existing IR names table for field names.

Add bytecode operation and `.cdbc` syntax:

```text
rD = assign_field rObject, nField, rValue
```

Both C++ bytecode debug output and `.cdbc` artifact output should be stable and covered by tests.

## Rust VM Parity

Update the Rust VM bytecode representation, parser/formatter, and executor to mutate existing struct fields in shared storage and return the assigned value.

Rust VM behavior must match C++ `--run` for:

- Basic field assignment.
- Nested field assignment.
- Alias-observed mutation.
- Assignment expression result.
- Runtime non-struct target errors.
- Missing-field errors.

## Tests

Success fixtures should cover:

- Basic field assignment and readback.
- Assignment expression result.
- Nested field assignment.
- Alias-observed mutation.
- Bytecode output for field assignment.
- Rust VM parity for supported success fixtures.

Parse-error fixtures should cover:

- Invalid assignment targets still reject.
- Missing value after `object.field =`.

Type-error fixtures should cover:

- Field assignment on statically known non-struct values.

Runtime-error fixtures should cover:

- Field assignment on dynamically non-struct unknown values.
- Assignment to a missing field.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` assignment target comments.
- `README.md` struct language section.
- `docs/roadmap.md` to mark Phase 12B implemented after implementation lands.
- `AGENTS.md` current language semantics.
- `docs/bytecode-text-format.md` for `assign_field`.
