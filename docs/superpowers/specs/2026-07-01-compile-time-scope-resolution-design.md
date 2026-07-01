# Compile-Time Scope Resolution Design

## Goal

Move lexical scope resolution out of the IR interpreter and into the compiler pipeline.

After this change, block scopes, shadowing, duplicate declarations, undefined variable reads, and undefined assignments are checked and resolved before IR compilation. The IR should use compiler-generated unique variable names, so the runtime interpreter no longer needs `BeginScope`, `EndScope`, or a scope stack.

## Current State

The current lexical-scope implementation represents scopes in IR:

- `IROp::BeginScope`
- `IROp::EndScope`
- `IRProgram::emitBeginScope()`
- `IRProgram::emitEndScope()`

`IRCompiler` emits these operations around `BlockStmt`, and `IRInterpreter` maintains a runtime stack of variable maps. Variable lookup and assignment are resolved at runtime by searching that stack.

`TypeChecker` already maintains a compile-time scope stack for type annotations and duplicate declarations, but unresolved variable reads and assignments currently fall back to `unknown` so runtime can still report undefined-variable errors.

## Chosen Approach

Extend `TypeChecker` into a combined type-checking and name-resolution pass.

Pipeline:

```text
Lexer -> Parser -> TypeChecker/Resolver -> IRCompiler -> IRInterpreter
```

`TypeChecker` will produce a `ResolvedNames` object that records compiler-generated names for AST nodes that bind or use variables. `IRCompiler` will consume `ResolvedNames` and emit IR using those unique names.

`IRInterpreter` will return to a flat variable map. It will keep defensive runtime checks for malformed IR, but normal compiler-produced IR will already have resolved all lexical scopes.

## ResolvedNames API

Add a resolved-name side table, likely in `include/TypeChecker.hpp`:

```cpp
class ResolvedNames {
public:
    const std::string& letName(const LetStmt& statement) const;
    const std::string& variableName(const VariableExpr& expression) const;
    const std::string& assignmentName(const AssignExpr& expression) const;

private:
    friend class TypeChecker;

    void recordLet(const LetStmt& statement, std::string name);
    void recordVariable(const VariableExpr& expression, std::string name);
    void recordAssignment(const AssignExpr& expression, std::string name);

    std::unordered_map<const LetStmt*, std::string> letNames_;
    std::unordered_map<const VariableExpr*, std::string> variableNames_;
    std::unordered_map<const AssignExpr*, std::string> assignmentNames_;
};
```

`TypeChecker` should expose the resolved names after a successful check:

```cpp
class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);
};
```

The returned reference is valid for the lifetime of the `TypeChecker` object and the AST whose node addresses were recorded.

## Compile-Time Binding Model

Replace `TypeChecker` scope entries from `StaticType` to a binding object:

```cpp
struct Binding {
    StaticType type;
    std::string resolvedName;
};
```

Scopes become:

```cpp
std::vector<std::unordered_map<std::string, Binding>> scopes_;
```

`TypeChecker` maintains a counter:

```cpp
std::size_t nextResolvedName_ = 0;
```

Resolved names should be stable and readable, for example:

```text
x#0
x#1
```

Rules:

- `let x = ...` declares a new binding in the current scope.
- Same-scope duplicate declaration is a type error.
- Inner scopes may shadow outer names.
- Variable reads search from innermost scope outward.
- Assignments search from innermost scope outward.
- Missing read or assignment target is a type error.

## Declaration Resolution

For each `LetStmt`:

1. Check the initializer expression first using the currently visible bindings.
2. Resolve the annotation, if present.
3. Validate initializer compatibility.
4. Check current scope for duplicate source name.
5. Generate a unique resolved name from the source lexeme and the counter.
6. Record `ResolvedNames::letName(statement)`.
7. Store `{type, resolvedName}` in the current scope.

This preserves the current rule that a variable is not visible inside its own initializer.

Example:

```cd
let x = x;
```

The initializer read of `x` is an undefined-variable type error unless an outer `x` exists.

## Variable and Assignment Resolution

For `VariableExpr`:

- Look up the nearest binding.
- If not found, throw:

```text
Type error: undefined variable `name`
```

- Record `ResolvedNames::variableName(expression)`.
- Return the binding's static type.

For `AssignExpr`:

- Check the RHS expression.
- Look up the nearest binding for the assignment target.
- If not found, throw:

```text
Type error: undefined variable `name`
```

- Check assignment compatibility when both sides are known.
- Record `ResolvedNames::assignmentName(expression)`.
- Return the target type if known, otherwise the value type.

## IR Compiler Changes

Change `IRCompiler` to require resolved names:

```cpp
class IRCompiler {
public:
    IRProgram compile(const Program& program, const ResolvedNames& resolvedNames);
};
```

Internally store a pointer or reference to `ResolvedNames` while compiling.

Variable lowering should use resolved names:

- `LetStmt` -> `resolvedNames.letName(*let)`
- `VariableExpr` -> `resolvedNames.variableName(*variable)`
- `AssignExpr` -> `resolvedNames.assignmentName(*assign)`

`BlockStmt` lowering should no longer emit scope instructions. It should simply compile child statements, because scope has already been encoded into unique names.

## IR Changes

Remove scope operations from IR:

- `IROp::BeginScope`
- `IROp::EndScope`
- `IRProgram::emitBeginScope()`
- `IRProgram::emitEndScope()`
- `irOpName` cases for `begin_scope` and `end_scope`

Update IR printing goldens so block-related programs show flat IR using unique variable names instead of scope boundary instructions.

## IR Interpreter Changes

Replace runtime scope stack with a flat map:

```cpp
std::unordered_map<std::string, Value> globals_;
```

`StoreVar` should declare or overwrite the generated internal name. Since internal names are unique for declarations, overwriting should not happen for compiler-produced IR, but `insert_or_assign` is acceptable.

`LoadVar` and `AssignVar` should keep defensive undefined-variable runtime checks. These protect the interpreter from malformed hand-built IR and should not normally trigger from compiler-produced code.

`globals()` should return the flat map.

## Error Migration

The following runtime-error fixtures should move to type-error fixtures:

- `undefined_variable`
- `assign_undefined`
- `block_local_escape`

All should use the error message:

```text
Type error: undefined variable `name`
```

Existing runtime-error fixtures that should remain runtime errors include:

- division by zero
- invalid add involving an unannotated `unknown` variable, intentionally kept dynamic

## Golden Test Expectations

Success IR goldens for lexical-scope cases should change from runtime scope instructions to unique names.

Example source:

```cd
let x = 1;
{
  let x = 10;
  x = 20;
  print x;
}
print x;
```

Expected IR shape:

```text
store_var @0 x#0, v0
store_var @1 x#1, v1
assign_var @2 x#1, v2
load_var @3 x#1
print v3
load_var @4 x#0
print v4
```

No `begin_scope` or `end_scope` instructions should remain in compiler-generated IR.

## Documentation Updates

Update `README.md` and `AGENTS.md` to state that lexical scopes are resolved at compile time and undefined variable reads/assignments are type errors.

Update `docs/roadmap.md` if useful to note that scope resolution has moved from runtime interpretation to the compiler/type-checker pass.

## Non-Goals

- Do not add full type inference for unannotated variables.
- Do not add functions, closures, loops, or modules.
- Do not change source-language syntax.
- Do not change runtime `Value` representation.
- Do not remove defensive undefined-variable checks from the interpreter.

## Success Criteria

- `IROp::BeginScope` and `IROp::EndScope` are removed.
- Compiler-produced IR contains no scope boundary instructions.
- Lexical shadowing still behaves correctly.
- Assignment updates the nearest resolved binding.
- Undefined reads, undefined assignments, and block-local escapes are type errors before IR compilation.
- Runtime interpreter no longer maintains lexical scope stack.
- Existing successful programs still pass.
- Full verification passes:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```
