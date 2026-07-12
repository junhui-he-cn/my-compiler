# Recursive Struct Rules Design

## Goal

Define explicit language behavior for recursive named struct field types. This
phase does not add recursive struct support. Instead, it replaces the current
confusing `unknown type` behavior for self-recursive declarations with clear
`Type` diagnostics and extends the rule to indirect recursion.

This is a language/type-checker slice. Syntax, AST shape, IR, bytecode, runtime
values, and VM execution behavior do not change for successful programs.

## User-Facing Behavior

Named struct field types must not recursively reference the struct being
declared, directly or indirectly. Unsupported recursive examples include:

```cd
struct Node { next: Node }
struct Node { next: Node? }
struct Node { children: [Node] }

struct A { b: B }
struct B { a: A }
```

Each should report a type error that identifies recursion rather than saying the
referenced type is unknown. A representative diagnostic is:

```text
Type error at 1:21: recursive struct field `next` references `Node`
```

For indirect recursion, the diagnostic should point at the field annotation that
closes the cycle and name the referenced type, for example:

```text
Type error at 2:15: recursive struct field `a` references `A`
```

Non-recursive unknown types should keep the current `unknown type` diagnostic.
Non-recursive forward struct field references within the same checking scope
become valid as part of the predeclaration model needed to detect indirect
cycles. Current non-recursive named struct declarations, constructors, field
access, field assignment, methods, imports, and exports should otherwise keep
their behavior.

## Scope

Include:

- clear type errors for direct recursive struct fields;
- clear type errors for recursive references nested inside nullable, array, and
  function type annotations;
- clear type errors for indirect struct cycles when all involved structs are
  declared in the same module/checking scope;
- support for non-recursive forward struct field references in the same checking
  scope;
- type-error golden fixtures for direct, nullable, array, function, and indirect
  recursion;
- one success fixture for a non-recursive forward struct field reference;
- README and roadmap updates.

Exclude:

- allowing recursive structs;
- changing constructor syntax or initialization rules;
- field creation by assignment;
- runtime named struct metadata or `typeOf` changes;
- dynamic dispatch, inheritance, overloading, protocols, or optional chaining;
- parser or grammar changes;
- IR, bytecode, `.cdbc`, or VM changes.

## Type Checker Architecture

Currently `checkStructDeclaration` resolves field annotations immediately before
inserting the struct into `structTypes_`. Because of that, direct recursion such
as `struct Node { next: Node? }` reports `unknown type Node`.

Change struct declaration checking to two phases within the type checker:

1. Predeclare struct names for the current checking scope before resolving field
   annotations.
2. Resolve each struct body and reject cycles in field types.

A small declaration-state table can track named structs during checking:

```cpp
enum class StructCheckState {
    Declared,
    Checking,
    Checked,
};
```

The type checker should mark a struct as `Checking` while resolving its fields.
If field type resolution sees a named struct whose state is `Checking`, it has
found a cycle and should report a recursive-field type error at the annotation
name token.

The implementation can be limited to struct field annotations. Function
parameters, function returns, and local variable annotations do not need a new
state machine beyond existing `findStructType` behavior, because this phase is
about recursive struct fields.

## Detecting Nested References

Cycle detection must inspect the full annotation tree, not only top-level field
names. The following all reference `Node` recursively and should be rejected:

```cd
struct Node { next: Node? }
struct Node { children: [Node] }
struct Node { makeNext: fun(): Node }
struct Node { acceptNext: fun(Node): nil }
```

Implementation options:

- add a `resolveStructFieldAnnotation` helper that walks `TypeAnnotation`,
  checks recursive references, then delegates to normal annotation resolution; or
- add a recursive-reference validation pass over `TypeAnnotation` before calling
  `resolveAnnotation`.

The helper should report the token for the named type annotation that closes the
cycle. For nullable, array, and function annotations, this is the nested named
annotation token (`Node` in `Node?`, `[Node]`, or `fun(): Node`).

## Direct and Indirect Cycles

Direct cycle:

```cd
struct Node { next: Node }
```

While checking `Node`, resolving field `next` sees `Node` in `Checking` state and
reports recursion.

Indirect cycle:

```cd
struct A { b: B }
struct B { a: A }
```

To support this without allowing forward references generally everywhere, struct
names in the same module/checking scope should be predeclared before bodies are
resolved. While checking `A`, the checker may need to recursively check `B` to
know whether `B` is safe. While checking `B`, seeing `A` in `Checking` state
reports recursion at `A` in field `a`.

Non-recursive forward struct references in field declarations become valid as a
consequence of predeclaration:

```cd
struct Box { value: Value }
struct Value { n: number }
```

This is a static type-checker improvement and does not change runtime
representation. The implementation should document that same-scope struct field
annotations may refer to struct names declared later in that scope.

## Error Messages

Use the existing type diagnostic shape. Suggested messages:

- direct/nested recursion:
  `recursive struct field `fieldName` references `StructName``
- indirect recursion:
  same message at the token that closes the cycle.

Examples:

```text
Type error at 1:21: recursive struct field `next` references `Node`
```

```text
Type error at 2:15: recursive struct field `a` references `A`
```

Do not introduce a new diagnostic category.

## Tests

Add type-error fixtures under `tests/golden/type_errors/`:

1. direct recursion:

   ```cd
   struct Node { next: Node }
   ```

2. nullable recursion:

   ```cd
   struct Node { next: Node? }
   ```

3. array recursion:

   ```cd
   struct Node { children: [Node] }
   ```

4. function return recursion:

   ```cd
   struct Node { makeNext: fun(): Node }
   ```

5. function parameter recursion:

   ```cd
   struct Node { acceptNext: fun(Node): nil }
   ```

6. indirect recursion:

   ```cd
   struct A { b: B }
   struct B { a: A }
   ```

Add one success fixture for a non-recursive forward struct field reference:

```cd
struct Box { value: Value }
struct Value { n: number }
let box = Box { value: Value { n: 1 } };
print box.value.n;
```

## Documentation

Update `README.md` named struct limitations to say recursive struct field types
are explicitly rejected for now, and document that same-scope struct field
annotations may refer to struct names declared later in that scope.

Update `docs/roadmap.md` after implementation to remove or narrow the recursive
struct rule item from Phase 12 future work.

Do not update `docs/language-grammar.ebnf`, because syntax is unchanged.

## Success Criteria

- Direct recursive struct fields produce a clear recursive-field type error.
- Nullable, array, and function annotation recursion produce clear
  recursive-field type errors at the nested struct name token.
- Indirect recursive struct cycles produce a clear recursive-field type error.
- Non-recursive same-scope forward struct field references are accepted.
- Non-recursive unknown field types outside the predeclared struct set still
  report `unknown type`.
- Existing named struct, method, import/export, and runtime fixtures continue to
  pass.
- No parser, IR, bytecode, `.cdbc`, or VM behavior changes are required for
  successful programs.
