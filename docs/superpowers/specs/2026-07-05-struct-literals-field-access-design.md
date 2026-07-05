# Struct Literals and Field Access Design

## Goal

Add the first Phase 12 aggregate-data slice: anonymous struct literals and field access.

This phase introduces runtime struct values and dot field reads while keeping named struct declarations, field assignment, methods, and static field-shape tracking out of scope.

## User-Visible Syntax

Struct literals use braces with identifier field names and expression values:

```cd
let person = { name: "Ada", age: 36 };
print person.name;
print person.age;
```

Field values may be any expression:

```cd
let nested = { point: { x: 1, y: 2 }, label: "p" };
print nested.point.x;
```

Field access uses dot syntax:

```cd
structValue.fieldName
```

## Terminology

Use **struct** throughout implementation, diagnostics, docs, IR, bytecode, and VM code for this feature.

This phase implements anonymous struct values only. Named declarations such as the following are future work:

```cd
struct Person { name: string, age: number }
```

## Scope

This phase includes:

- Struct literal parsing: `{ name: expression, ... }` and `{}`.
- Field access parsing: `expression.identifier`.
- AST nodes and AST printer output for struct literals and field access.
- Runtime struct value representation with stable field order for printing.
- Static `struct` top-level type recognition.
- Type errors for statically known non-struct field access.
- Runtime errors for dynamic non-struct field access and missing fields.
- IR, bytecode, `.cdbc`, and Rust VM execution parity.
- Golden tests and documentation.

This phase does not include:

- Field assignment: `person.age = 37`.
- Named struct declarations.
- Struct type annotations such as `{ name: string }` or `struct Person`.
- Static tracking of field names or field value types.
- Methods or dot/member calls such as `xs.push(value)`.
- Spread, computed field names, destructuring, or field shorthand.

## Grammar

Add `.` as a token and extend call/postfix syntax:

```ebnf
call        = primary,
              { "(", [ arguments ], ")"
              | "[", expression, "]"
              | ".", identifier } ;
```

Add struct literals to primary expressions:

```ebnf
struct      = "{", [ fields ], "}" ;

fields      = field,
              { ",", field } ;

field       = identifier, ":", expression ;

primary     = functionExpr
            | "false"
            | "true"
            | "nil"
            | number
            | string
            | array
            | struct
            | identifier
            | "(", expression, ")" ;
```

Do not allow trailing commas in this phase, matching current array/call argument behavior.

## AST Shape and Printing

Add:

```cpp
struct StructField {
    Token name;
    ExprPtr value;
};

struct StructExpr final : Expr {
    explicit StructExpr(std::vector<StructField> fields);
    void print(std::ostream& out) const override;

    std::vector<StructField> fields;
};

struct FieldAccessExpr final : Expr {
    FieldAccessExpr(ExprPtr object, Token name);
    void print(std::ostream& out) const override;

    ExprPtr object;
    Token name;
};
```

AST printer output examples:

```text
Let person = (struct name: "Ada" age: 36)
Print (field person name)
Let nested = (struct point: (struct x: 1 y: 2) label: "p")
Print (field (field nested point) x)
```

## Static Type Checking

Add `StaticType::Struct`.

Rules:

- Struct literal expressions have static type `struct`.
- Field values are type-checked, but field names/types are not tracked.
- Field access on a known non-struct, non-unknown value is a type error.
- Field access on a known struct has result type `unknown`.
- Field access on an unknown value has result type `unknown` and is checked at runtime.

Suggested type diagnostic:

```text
Type error at 1:10: can only access fields on structs
```

## Runtime Semantics

Struct values are reference values, like arrays and functions.

A struct stores:

- A stable identity for equality.
- Field values keyed by name.
- Field insertion order for stable printing.

Printing preserves literal order:

```text
{name: Ada, age: 36}
```

Equality is identity-based, matching arrays and functions.

Field access:

- If the runtime value is not a struct, raise:

```text
Runtime error: can only access fields on structs
```

- If the field does not exist, raise:

```text
Runtime error: undefined field `missing`
```

## IR and Bytecode

Add IR operations:

- `Struct`: builds a struct from ordered field name/value pairs.
- `Field`: reads a named field from a struct value.

Use the existing names table for field names where possible.

Suggested IR print shape:

```text
v2 = struct {name: v0, age: v1}
v3 = field v2.name
```

Add matching bytecode operations and `.cdbc` text format entries, for example:

```text
r2 = struct {n0: r0, n1: r1}
r3 = field r2, n0
```

Exact bytecode text syntax can be adjusted during planning, but it must be stable, documented, and supported by both the C++ emitter and Rust parser/formatter.

## Rust VM Parity

Update Rust VM runtime values to support structs with shared field storage and stable printing.

The Rust VM must match C++ `--run` for:

- Struct literal printing.
- Field access.
- Nested field access.
- Runtime non-struct field errors.
- Missing field errors.

## Testing

Success golden fixtures should cover:

- Basic struct literal field access.
- Empty struct printing.
- Nested struct field access.
- Struct values stored in variables and passed through functions.
- Identity equality for aliases vs separately constructed structs.
- Bytecode output for struct creation and field reads.
- Rust VM parity for supported success fixtures.

Parse-error fixtures should cover:

- Missing field name.
- Missing colon after a field name.
- Missing expression after a colon.
- Missing closing brace.
- Trailing comma rejection.
- Missing field name after dot.

Type-error fixtures should cover:

- Field access on a statically known number/string/bool/nil/function/array.

Runtime-error fixtures should cover:

- Field access on a dynamically non-struct unknown value.
- Missing field access on a struct.

## Documentation Updates

Update:

- `docs/language-grammar.ebnf` for struct literals and dot field access.
- `README.md` for user-visible struct values and field reads.
- `docs/roadmap.md` to mark Phase 12A implemented after implementation lands.
- `AGENTS.md` current language semantics.
- `docs/bytecode-text-format.md` if bytecode text syntax changes.
