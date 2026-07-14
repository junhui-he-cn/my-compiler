# Explicit Generic Call Arguments Design

Date: 2026-07-14
Status: approved for implementation planning

## Goal

Allow callers to select concrete types for the existing generic named-function
slice when inference is insufficient or when the caller wants to make the
specialization explicit.

## User-visible syntax and scope

Generic calls use the existing angle-bracket tokens immediately before the
argument list:

```cd
fun identity<T>(value: T): T {
  return value;
}

print identity<number>(42);
print identity<string>("hello");
```

The same syntax works through a namespace import:

```cd
print lib.identity<number>(42);
```

An explicit call supplies one type argument for every declared type parameter.
Partial type-argument lists are not supported. The type arguments may use the
existing annotation forms: primitive types, named struct and enum types,
qualified types, arrays, maps, function types, nullable types, and type
parameters already in scope.

Explicit arguments are supported for generic named functions and aliases to
generic named functions. Generic methods, generic function expressions, and
generic containers beyond the existing `map<K, V>` annotation remain out of
scope. Explicit arguments on builtins, enum constructors, ordinary methods, or
non-generic functions are type errors.

The parser recognizes the canonical compact postfix form `name<T>(...)` or
`namespace.name<T>(...)`. At a postfix expression position, a balanced angle
bracket sequence followed by `(` is treated as a type-argument list only when
the `<` is directly adjacent to the callee name. This keeps ordinary comparison
expressions such as `left < right` and comparison chains without a generic-call
shape unchanged.

## Static semantics

For a call with explicit type arguments:

1. Resolve each type annotation with the existing `TypeChecker::resolveAnnotation`
   rules.
2. Require the callee to have a complete function signature and a non-empty
   generic-parameter list.
3. Require the number of supplied type arguments to equal the number of generic
   parameters.
4. Bind generic parameter names to the supplied `TypeInfo` values.
5. Check arguments against the function parameter types after recursive
   substitution.
6. Return the recursively substituted function return type.

Explicit arguments replace inference; the call must not infer a different type
from its runtime argument. A supplied type parameter can itself be a type
parameter from an enclosing generic function, and recursive array, map,
nullable, and function shapes retain the existing substitution behavior.

No runtime specialization, monomorphization, new IR operation, bytecode opcode,
or VM type tag is introduced. The type arguments are erased before IR lowering.

## Diagnostics

Use the existing Type diagnostic category and call-site locations:

- A non-generic callee reports `function is not generic`.
- A callee without a complete known function signature reports
  `explicit type arguments require a known function signature`.
- A wrong number of type arguments reports
  `expected N type arguments but got M`.
- An unknown type name is reported by the existing annotation resolver at the
  type-argument token.
- Argument mismatches use the existing
  `argument N expects <type>, got <type>` diagnostic after substitution.

## AST and parser representation

`CallExpr` and `MemberCallExpr` carry `std::vector<TypeAnnotation>
typeArguments`. The AST printer includes a supplied list after the callee name,
for example `(call identity<number> 42)`. The parser builds this metadata and
does not create runtime expression nodes for type arguments.

## Testing strategy

Add a success golden covering primitive, recursive array, return-only, alias,
and namespace calls. Its AST output proves the syntax is preserved while its
IR, bytecode, C++ run, and Rust VM run outputs prove that runtime calls remain
ordinary calls.

Add type-error goldens covering wrong arity, non-generic calls, an explicit type
that conflicts with an argument, and an unknown type argument. Add a parser
regression for a comparison chain if the postfix lookahead changes its
interpretation.

Update `README.md`, `docs/language-grammar.ebnf`, and `docs/roadmap.md` to
document the implemented explicit-call syntax and remove it from the remaining
generic type-system work.
