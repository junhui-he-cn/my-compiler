# Struct Methods Design

## Goal

Strengthen member calls by adding a first user-defined method slice for named structs. This is Phase 12F after builtin member-call sugar.

The first implementation should be intentionally small: statically resolve methods on known named struct receiver types and lower calls to ordinary function calls with the receiver passed as the first argument. It should not introduce dynamic dispatch, inheritance, interfaces, or new bytecode opcodes.

## User-Facing Semantics

Add `impl` blocks for named structs:

```cd
struct Person { name: string, age: number }

impl Person {
  fun greet(): string {
    return this.name;
  }

  fun birthday(): nil {
    this.age = this.age + 1;
    return nil;
  }
}

let p: Person = Person { name: "Ada", age: 36 };
print p.greet();
p.birthday();
print p.age;
```

Methods are called with existing member-call syntax:

```cd
receiver.method(arg1, arg2)
```

Inside a method body, `this` is an implicit variable bound to the receiver. `this` has the named struct type from the enclosing `impl` block. Because runtime structs are reference values, mutating `this.field` mutates the receiver object and aliases observe the change.

## First Slice Scope

Included in the first implementation plan:

- Parse top-level `impl Type { fun method(...) ... }` declarations.
- Support methods only on local named structs declared with `struct Name { ... }`.
- Add `this` as an implicit method-body binding.
- Type-check method declarations, parameters, return annotations, and method bodies using existing function rules.
- Type-check `receiver.method(args...)` when receiver has a statically known named struct type.
- Lower method declarations to hidden functions whose first parameter is the receiver.
- Lower `receiver.method(args...)` to a normal function call with receiver prepended to explicit arguments.
- Preserve existing builtin member-call sugar and namespace calls.
- Add AST, IR, run, type-error, bytecode artifact, Rust VM parity, grammar, README, roadmap, and AGENTS coverage.

Excluded from the first slice:

- Methods on anonymous structs.
- Methods on imported/namespaced structs.
- Exporting or importing methods.
- Re-exporting methods.
- Dynamic dispatch for unknown receiver types.
- Inheritance, interfaces, protocols, traits, or virtual methods.
- Method overloading by arity or type.
- Generic methods.
- Static methods or constructors.
- Capturing `this` outside a method body as a special lifetime feature.
- Function-valued field calls such as `object.field()`.
- Optional chaining such as `receiver?.method()`.
- New bytecode opcodes.

## Grammar

Add `implDecl` as a top-level declaration form:

```ebnf
declaration = importDecl
            | exportDecl
            | structDecl
            | implDecl
            | funDecl
            | letDecl
            | statement ;

implDecl    = "impl", identifier,
              "{", { methodDecl }, "}" ;

methodDecl  = "fun", identifier,
              "(", [ parameters ], ")",
              [ ":", typeExpr ],
              block ;
```

`impl` should be a reserved keyword/token. If this creates parse diagnostics for existing uses of `impl` as an identifier, that is acceptable for this language phase and should be covered by parser tests only if existing fixtures depend on `impl` names.

Method calls use the already implemented member-call grammar:

```ebnf
memberCall = ".", identifier, "(", [ arguments ], ")" ;
```

## AST and Printing

Add a top-level statement node:

```cpp
struct MethodDecl {
    Token name;
    std::vector<Parameter> parameters;
    std::optional<TypeAnnotation> returnTypeName;
    std::vector<StmtPtr> body;
};

struct ImplStmt final : Stmt {
    Token typeName;
    std::vector<MethodDecl> methods;
};
```

AST printing should be stable and explicit. Suggested shape:

```text
Impl Person
  Method greet(): string
    Return (field this name)
```

Use the project's current AST indentation conventions rather than inventing a separate format.

## Type Checking

### Method Table

During type checking, create a method table keyed by named struct type and method name:

```text
Person.greet -> function signature with implicit receiver
Person.birthday -> function signature with implicit receiver
```

Validation rules:

- `impl Name` requires `Name` to be a known local struct type.
- Duplicate method names in the same struct are type errors.
- A method name cannot conflict with a field name in that struct for the first slice.
- Method parameters follow normal function parameter rules, including duplicate-parameter errors.
- Method return annotations follow normal function return rules.
- `this` is available only inside method bodies.
- `this` outside a method body is an undefined-variable type error, unless a user explicitly declares a normal variable named `this` in scope.
- Declaring a method parameter or same-scope local named `this` inside a method is a duplicate declaration/type error because the implicit receiver already uses that name in the method scope.

### Method Calls

`MemberCallExpr` type checking order should become:

1. If receiver is a namespace alias, preserve current `alias.exportedValue(...)` behavior.
2. If the member name is a supported builtin member (`push`, `pop`, `len`, `substr`, `charAt`), preserve current builtin member-call behavior.
3. If receiver has statically known named struct type, look up the method in that struct's method table.
4. Otherwise report a type error for unknown member calls. The first slice does not attempt runtime method lookup for unknown receiver types.

For named struct method calls:

- The receiver expression is type checked once and must have known `StaticType::Struct` with a `structName`.
- The method must exist on that struct.
- Explicit argument arity must match the method's declared parameter count, excluding `this`.
- Explicit argument types must be compatible with method parameter annotations when known.
- The call expression type is the method return type. Unannotated method returns use existing conservative return inference rules.

Suggested diagnostics:

```text
Type error at 1:1: unknown struct type `Missing` in impl
Type error at 1:1: duplicate method `greet` for struct `Person`
Type error at 1:1: method `name` conflicts with field `name` on struct `Person`
Type error at 1:1: struct `Person` has no method `missing`
Type error at 1:1: can only call methods on known named structs
```

Exact token positions should follow existing diagnostic conventions and can be finalized during implementation.

## Lowering and Runtime Model

No new runtime method representation is required.

Each method lowers to a hidden function with a synthetic name, for example:

```text
Person.greet(this, explicitArgs...)
```

The hidden function's first parameter should be the resolved `this` binding. Method calls lower to ordinary IR calls:

```cd
p.greet(1, 2)
```

becomes conceptually:

```cd
__method_Person_greet(p, 1, 2)
```

Receiver evaluation order:

1. Evaluate receiver exactly once.
2. Evaluate explicit arguments left-to-right.
3. Call the hidden method function.

This matches builtin member-call receiver ordering and avoids new IR or bytecode opcodes.

Because runtime structs are reference values, existing field access and field assignment operations are sufficient for `this.field` reads and writes.

## Interactions with Existing Member Calls

Builtin member calls remain special and are not shadowed by lexical variables. For the first slice, builtin names continue to win before struct methods. This means a struct method named `len`, `push`, `pop`, `substr`, or `charAt` is either rejected as a method name or unreachable by member-call syntax. To keep behavior simple, the first implementation should reject method names that conflict with builtin member-call names.

Namespace calls such as `math.add(2, 3)` must keep working. They are represented as `MemberCallExpr` by the parser today, so type checking and lowering must continue to recognize namespace receiver aliases before treating the expression as a builtin or struct method call.

## Modules

Method export/import behavior is explicitly deferred.

For the first slice:

- `impl` blocks in imported files may be type-checked as part of that module, but their methods are private to that module unless a later phase defines export behavior.
- Direct use of methods across module boundaries is not supported.
- `import "path" as alias; alias.Type { ... }` constructor syntax remains unrelated to method lookup.

A later Phase 12G or Phase 14 follow-up should decide whether exporting a struct also exports its methods.

## Tests

Success fixtures:

- Parse/AST for a simple `impl` block.
- Method reads a field through `this` and returns a string/number.
- Method mutates a field through `this.field = ...`; aliases observe mutation.
- Method with explicit parameters and return type.
- Method call result used in expressions and assignments.
- Existing builtin member calls still work.
- Existing namespace calls still work.

Type-error fixtures:

- `impl Missing { ... }` unknown struct.
- Duplicate method in same `impl` or across two `impl` blocks for the same struct.
- Method name conflicts with a field.
- Method name conflicts with builtin member-call names.
- `this` outside method is undefined unless explicitly declared as a normal variable.
- Method parameter named `this` is duplicate/conflicting.
- Unknown method on known struct.
- Method call on unknown receiver type rejected for user methods.
- Wrong method arity.
- Wrong method argument type.
- Method return type mismatch.

Runtime/bytecode/Rust VM coverage:

- At least one method-read fixture.
- At least one method-mutation fixture.
- Bytecode artifact fixture proving lowering uses functions/calls and existing struct field ops.
- Rust VM parity for the same fixtures.

## Documentation

Update:

- `README.md`: document `impl`, `this`, supported method calls, and first-slice exclusions.
- `docs/language-grammar.ebnf`: add `implDecl` and `methodDecl`.
- `docs/roadmap.md`: add Phase 12F as implemented/in progress after completion; keep modules/exported methods as future work.
- `AGENTS.md`: update current language semantics and limitations.

## Open Decisions for Later Phases

These are deliberately postponed:

- Whether exporting a struct exports its methods.
- Whether imported/namespaced structs can use methods through namespace aliases.
- Whether builtin member-call names should always win or whether struct methods can override them.
- Whether function-valued fields should be callable with `object.field()`.
- Whether methods can be first-class values.
- Whether method declarations can be placed inside `struct` declarations instead of `impl` blocks.
