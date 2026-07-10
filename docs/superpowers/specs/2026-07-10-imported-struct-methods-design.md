# Imported Struct Methods Design

## Goal

Support statically resolved method calls on exported/imported named structs, including direct imports and namespace imports. This completes the next M1 language-semantics slice without changing runtime struct representation, bytecode format, dynamic dispatch, inheritance, or method call lowering semantics.

## Current State

Local named struct methods already work:

```text
struct Counter { value: number }
impl Counter { fun inc(delta: number): number { ... } }
let c: Counter = Counter { value: 1 };
print c.inc(4);
```

The type checker stores local method metadata in `methods_`, and IR lowering compiles each method as an ordinary function value stored under a hidden resolved name. A member call on a known named struct lowers to a normal function call with the receiver passed as the first implicit `this` argument.

Modules currently export value bindings and named struct field shapes, but not method metadata. Therefore, importing an exported struct preserves fields and constructor typing but loses its associated method table.

## Scope

In scope:

- Direct imports:

  ```text
  import "./lib.cd";
  let p: Point = Point { x: 3, y: 4 };
  print p.length();
  ```

- Namespace imports:

  ```text
  import "./lib.cd" as shapes;
  let p: shapes.Point = shapes.Point { x: 3, y: 4 };
  print p.length();
  ```

- Exported method signatures and hidden method resolved names for exported structs.
- Static argument count/type checks for imported methods.
- Static return type propagation from imported methods.
- Runtime execution parity through the existing IR → bytecode → Rust VM path.
- Golden coverage for success, type-error, and Rust VM parity.

Out of scope:

- Dynamic dispatch, inheritance, protocols, overloading, and method visibility modifiers.
- Adding `impl ImportedType { ... }` in importing modules.
- Exporting methods independently of their struct type.
- Re-export syntax.
- Separate compilation or serialized module interface artifacts.
- Runtime named struct values or runtime type names beyond the current generic `struct` behavior.
- Bytecode opcode or `.cdbc` format changes.

## Proposed Architecture

### Shared Method Metadata

Introduce a data-only method metadata type that can cross module boundaries without carrying AST pointers:

```cpp
struct MethodSignature {
    TypeInfo receiverType;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
    std::string resolvedName;
};
```

`TypeChecker::MethodInfo` remains private and keeps `const MethodDecl* declaration` for checking method bodies. Imported/exported method metadata uses `MethodSignature` only.

### ModuleSymbols Extensions

Extend `ModuleSymbols` to store method exports next to struct exports:

```cpp
using StructMethodTable = std::unordered_map<std::string, MethodSignature>;
using ModuleMethodExports = std::unordered_map<std::string, StructMethodTable>;
```

`NamespaceImport` should also carry method exports for namespaced structs:

```cpp
struct NamespaceImport {
    ModuleValueExports values;
    ModuleStructExports structs;
    ModuleMethodExports methods;
};
```

New `ModuleSymbols` API should support recording and retrieving method exports per module.

### Export Semantics

When `export Point;` exports a local struct, the checker should also snapshot the currently known method signatures for `Point` into `ModuleSymbols`.

A method is exported only through its exported struct. There is no standalone method export syntax. If a module defines `impl Point` but does not export `Point`, importing modules cannot access `Point` or its methods.

### Direct Import Semantics

For `import "./lib.cd";`, the type checker should import:

- exported values into the current top-level scope;
- exported struct shapes into `structTypes_` under their original names;
- exported method signatures into `methods_` under their original struct names.

If an imported struct conflicts with an existing local struct, the existing duplicate-struct diagnostic still applies before importing methods. If method conflicts arise after a valid struct import, report duplicate method diagnostics using the import token location.

### Namespace Import Semantics

For `import "./lib.cd" as lib;`, namespace import should copy method exports into the namespace metadata. During namespace alias declaration:

- exported struct `Point` is registered as `lib.Point`;
- exported methods for `Point` are registered under `methods_["lib.Point"]`;
- each method receiver type is rewritten from `Point` to `lib.Point`;
- method parameter and return types that refer to exported structs from the same namespace must also be qualified so signatures stay usable from the importing module.

The method `resolvedName` should not be rewritten. It must keep the hidden resolved name generated while checking the imported module, because IR lowering already compiles that method function and member-call lowering can load the same resolved name.

### Type Rewriting Rules

Namespace imports need a focused type-qualification helper for method signatures. It should recursively rewrite known named struct references that are exported by the namespace:

- `Point` → `lib.Point`
- `Point?` → `lib.Point?`
- `[Point]` → `[lib.Point]`
- `fun(Point): Point` → `fun(lib.Point): lib.Point`

Types not referring to exported namespace structs remain unchanged. Direct imports do not rewrite method signature types.

### IR and Runtime Behavior

No IR opcodes, bytecode opcodes, `.cdbc` formatting, or Rust VM behavior should change. Imported method calls should continue to lower exactly like local method calls:

```text
vMethod = load_var __method_Point_length#N
vReceiver = load_var p#M
vResult = call vMethod(vReceiver)
```

Namespaced method calls use the same hidden `resolvedName` produced by the imported module while the receiver type key used for type checking is namespaced.

## Diagnostics

Preserve existing diagnostic categories:

- Unknown imported method: type error, e.g. `struct `Point` has no method `missing`` or `struct `lib.Point` has no method `missing``.
- Wrong arity/type: same wording as local method calls.
- Import conflicts: use existing duplicate struct/namespace conflict diagnostics where possible.

Do not introduce runtime errors for statically known method-resolution failures.

## Testing Strategy

Add success fixtures with `run.out`, `ir.out`, and/or `bytecode.out` where useful, plus Rust VM parity coverage through `tests/run_rust_vm_tests.py --goldens`:

1. Direct import of a struct with a method returning a number.
2. Namespace import of a struct with a method returning a number.
3. Imported method mutates `this` and aliases observe the mutation.
4. Imported method signature checks argument types.
5. Namespaced method signature rewrites struct parameter/return types.
6. Non-exported struct/method remains inaccessible through existing private-name diagnostics.
7. Unknown imported/namespaced method reports a type error.

Add focused C++ tests for `ModuleSymbols` method table storage if the API grows enough to merit direct coverage.

Before completion, run the full repository verification suite from AGENTS.md and the sanitizer CTest path if sanitizer-affecting C++ code changes.

## Documentation Impact

Update `README.md` and `docs/language-grammar.ebnf` only if user-visible documented behavior changes there. The grammar does not change. README should mention that methods on exported/imported structs are supported if it has a language feature list for structs/modules.

## Risks and Mitigations

- **Risk:** Storing AST-backed `MethodInfo` in module exports could dangle after module checking restores local state.
  **Mitigation:** export only data-only `MethodSignature` values.
- **Risk:** Namespace imports may leave method parameter/return types unqualified, causing false type errors.
  **Mitigation:** add a recursive type rewriting helper and coverage for struct-typed method parameters/returns.
- **Risk:** Importing methods before imported module methods are checked could produce missing metadata.
  **Mitigation:** keep `checkModule(*imported)` before reading module exports, and record method exports when the struct export is processed after module statements have been checked.
- **Risk:** Method hidden function names could be rewritten incorrectly for namespace imports.
  **Mitigation:** never rewrite `resolvedName`; only rewrite type keys and `TypeInfo` struct names.

## Self-Review

- Placeholder scan: no placeholders, TODOs, or unresolved decisions remain.
- Internal consistency: direct imports preserve names; namespace imports qualify type-facing names but preserve hidden resolved method names.
- Scope check: this is one vertical language slice focused on imported/namespaced struct methods.
- Ambiguity check: export behavior, namespace rewriting, diagnostics, runtime behavior, and out-of-scope features are explicit.
