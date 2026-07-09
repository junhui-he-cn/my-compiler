# Nullable Types Design

## Goal

Add an explicit nullable type syntax `T?` so the static checker can distinguish values that may be `nil` from values that must not be `nil`.

This is Phase 9G from the language roadmap. It is a type-checking feature only: runtime values remain unchanged, and `nil` remains the existing runtime nil value.

## User-Facing Semantics

A nullable type `T?` means a value may be either `nil` or a value compatible with `T`.

Examples:

```cd
let age: number? = nil;
age = 42;
age = nil;

let name: string? = "Ada";
let values: [number?] = [1, nil, 3];
let maybeValues: [number]? = nil;
```

Function parameters and returns may be nullable:

```cd
fun findScore(name: string): number? {
  if name == "Ada" {
    return 100;
  }
  return nil;
}

fun greet(name: string?) {
  if name == nil {
    print "anonymous";
  } else {
    print name;
  }
}
```

Named structs may also be nullable, and function signatures can use nullable
parameter or return types:

```cd
struct Person { name: string }
let maybePerson: Person? = nil;
let callback: fun(number?): string? = fun (value: number?): string? {
  if value == nil {
    return nil;
  }
  return "ok";
};
```

## Compatibility Rules

Let `T` be any non-nullable known type such as `number`, `bool`, `string`, `[number]`, `Person`, or `fun(number): string`.

Allowed:

- `nil` is compatible with `T?`.
- `T` is compatible with `T?`.
- `T?` is compatible with `T?` when the wrapped `T` types are compatible.
- Unknown dynamic types remain compatible with nullable types, matching current conservative unknown-type behavior.

Rejected:

- `nil` is not compatible with non-nullable `T`.
- `T?` is not compatible with non-nullable `T`.
- `U` is not compatible with `T?` when `U` is a known non-`nil` type incompatible with `T`.
- `U?` is not compatible with `T?` when `U` is incompatible with `T`.

Examples:

```cd
let a: number? = nil; // OK
let b: number? = 1;   // OK
let c: number = nil;  // Type error
let d: number = a;    // Type error: a may be nil
let e: string? = 1;   // Type error
```

## Scope

Included:

- Add lexical support for `?`.
- Add nullable type annotations using postfix syntax: `typeExpr?`.
- Support nullable annotations anywhere existing type annotations are supported:
  - `let` annotations
  - function parameters
  - function returns
  - function type parameter and return positions
  - array element types
  - struct field declarations
  - qualified/namespaced struct types
- Update static compatibility checks so nullable assignments, arguments, returns, array literals, index assignments, `push`, struct literals, and field assignment use the rules above.
- Update type names in diagnostics and printed docs to include `?`, for example `number?`, `[string?]`, and `fun(number): string?`.
- Add parse/type/success golden coverage.
- Update `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, and `AGENTS.md`.

Excluded:

- Flow-sensitive narrowing after `x != nil` or `x == nil` checks.
- Pattern matching or unwrap syntax.
- Null-coalescing operators such as `??`.
- Optional chaining such as `value?.field`.
- Runtime representation changes.
- Changing `typeOf(nil)`, equality, truthiness, or print behavior.

## Syntax

Nullable is a postfix marker on type expressions:

```ebnf
typeExpr     = nullableType ;
nullableType = primaryType, [ "?" ] ;
primaryType  = arrayType
             | functionType
             | qualifiedType
             | simpleType ;
```

Postfix `?` binds to the immediately preceding type expression:

- `[number]?` means the array itself may be nil.
- `[number?]` means the array is non-nil, but its elements may be nil.
- `fun(): number?` means a non-nil function returning `number?`.
- `fun(number?): string` means a non-nil function accepting a nullable number.

Because existing function type syntax uses `fun(...): returnType`, this first slice supports nullable function return and parameter types but does not add parentheses for making the function value itself nullable. For `let maybeCallback: fun(number): string? = nil;`, `nil` is rejected because the annotation means a non-null function returning nullable string. A future type-parentheses slice can add `(fun(number): string)?` if nullable function values are needed.

## Type Model

Add a nullable wrapper to `TypeInfo`, for example:

```cpp
std::shared_ptr<TypeInfo> nullableOf;
```

A nullable type should keep its wrapped type as a full `TypeInfo` so it can wrap arrays, structs, and function signatures without flattening their metadata.

Helper functions should keep the logic centralized:

- `nullableType(TypeInfo inner)` creates `inner?`.
- `isNullable(type)` checks for the wrapper.
- `nonNullable(type)` or direct access retrieves the wrapped type.
- `typeInfoName(type)` prints nullable types by appending `?` to the wrapped type name.

The implementation should avoid nested nullable types in normal parsing. If `T??` is parsed accidentally through future syntax, diagnostics should reject it or normalize it to `T?`; this slice should reject extra `?` through the grammar/parser.

## Compatibility Algorithm

The existing `compatible(expected, actual)` stays the single source of assignment compatibility. Extend it before the current kind-equality check:

1. Unknown expected or actual remains compatible.
2. If `expected` is nullable:
   - `actual == nil` is compatible.
   - `actual` nullable is compatible if their inner types are compatible.
   - otherwise `actual` is compatible if it is compatible with the expected inner type.
3. If `expected` is not nullable and `actual` is nullable, reject.
4. Existing exact-kind, struct, array, and function compatibility logic remains unchanged for non-nullable types.

This makes nullable compatibility flow through all existing checks that already call `compatible`.

## Diagnostics

Use existing diagnostic sites. Messages should naturally include nullable type names through `typeInfoName`.

Examples:

```text
Type error at 1:5: cannot initialize `x` of type number with nil
Type error at 2:1: cannot assign number? to `x` of type number
Type error at 1:17: argument 1 expects number, got number?
Type error at 2:3: cannot return nil from function returning number
```

Exact line/column output should follow current diagnostic conventions and be locked by golden tests.

## Testing Strategy

Add success fixtures for:

- `let` initialization and assignment with `number?`.
- Function returns annotated `number?` returning both a number and `nil`.
- Parameters annotated `string?` accepting strings and nil.
- Array element type `[number?]` accepting nil elements and `push(xs, nil)`.
- Array variable type `[number]?` accepting nil for the whole array.
- Struct fields annotated `number?` accepting nil in literals and assignments.
- Named struct nullable annotations such as `Person?` accepting nil.

Add type-error fixtures for:

- `let x: number = nil;` remains rejected.
- Assigning `number?` to `number` is rejected.
- Passing `number?` to a parameter requiring `number` is rejected.
- Returning `nil` from a non-nullable return remains rejected.
- Assigning `string` to `number?` is rejected.
- Assigning `nil` to a non-nullable struct field remains rejected.

Add parse-error fixtures for malformed nullable syntax if the parser can report them cleanly, especially `let x: number?? = nil;`.

No new IR, bytecode, runtime, or Rust VM tests are required beyond existing runtime parity for programs that type-check, because nullable does not change runtime execution.

## Documentation

Update:

- `docs/language-grammar.ebnf`: add nullable type grammar.
- `README.md`: replace “nullable type syntax is not implemented yet” with the implemented `T?` rules and examples.
- `docs/roadmap.md`: mark Phase 9G implemented for explicit nullable annotations while leaving flow-sensitive narrowing as future work.
- `AGENTS.md`: update current type-system semantics and limitations.
