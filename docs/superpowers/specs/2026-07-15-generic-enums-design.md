# Generic Enums Design

## Scope

Add nominal generic enum declarations and instantiated enum types to the
existing enum, constructor, pattern, module-interface, and bytecode pipeline.
The first slice supports declarations such as `enum Option<T> { Some(T), None }`
and recursive payloads such as `List<T>`.

## Semantics

- Generic enum declarations use the existing type-parameter syntax:
  `enum Result<T, E> { Ok(T), Err(E) }`.
- Type annotations instantiate generic enums with angle-bracket arguments, for
  example `Result<number, string>` and `List<number>?`.
- Variant constructors infer generic arguments from payload expressions and,
  when available, the expected result type. A constructor may provide all
  explicit arguments on the variant call, such as `Option.None<number>()`.
- Constructors reject missing or conflicting inferred arguments and validate
  every payload against the instantiated payload type.
- Variant patterns use the scrutinee's instantiated enum type; recursive and
  named-payload patterns therefore receive substituted payload types.
- Generic enum type compatibility is nominal and invariant in each type
  argument. Generic enum declarations require all type arguments at use sites.
- Runtime enum values retain only the existing enum and variant names plus
  payload values. Generic arguments are compile-time metadata and require no
  new IR opcode, bytecode opcode, or `.cdbc` format change.
- Exported/imported enum metadata preserves generic parameters and instantiated
  payload types through direct imports, namespace imports, and module
  interfaces.

Constraints, defaults, higher-kinded types, generic structs, and generic enum
  methods are outside this slice.

## Implementation

Type annotations carry generic arguments, `TypeInfo` carries instantiated enum
  arguments, and `EnumTypeDecl`/module interfaces carry declaration parameter
  names. The checker substitutes declaration type parameters while checking
  constructors and patterns. IR lowering continues to use the erased runtime
  enum name already recorded by `ResolvedNames`.

## Verification

Add successful goldens for inferred/explicit constructors, recursive generic
patterns, nullable generic enums, and imported generic enums. Add type-error
coverage for wrong arity, constructor mismatch, duplicate parameters, and
uninferable unit constructors, plus bytecode artifact and Rust VM parity.
