# Lexical Scope Design

## Goal

Implement Phase 1 from `docs/roadmap.md`: make blocks introduce lexical scopes for variables while preserving the existing language surface syntax.

After this change, variables declared with `let` inside a block are only visible inside that block, inner blocks may shadow outer variables, assignment updates the nearest existing binding, and duplicate declarations in the same scope are errors.

## Current State

Currently `BlockStmt` only groups statements for control flow. `IRCompiler::compileStatement` compiles block children directly, and `IRInterpreter` stores all variables in a single `globals_` map. This means block-local variables leak and `let` can overwrite an existing name.

## Chosen Approach

Use runtime lexical scopes represented in IR.

Add two IR operations:

- `BeginScope`
- `EndScope`

The IR compiler emits these operations around each block statement. The IR interpreter maintains a stack of scopes:

```cpp
std::vector<std::unordered_map<std::string, Value>> scopes_;
```

The first scope is the global scope. Each block pushes a new scope on entry and pops it on exit.

This approach is intentionally smaller than a compile-time resolver. It fits the current interpreter architecture and can later be replaced or supplemented by a resolver/type checker.

## Runtime Semantics

### Global Scope

Program execution starts with one global scope. Top-level `let` declarations live in that global scope.

### Block Scope

A block creates a nested lexical scope:

```cd
let x = "global";
{
  let x = "inner";
  print x;
}
print x;
```

Expected output:

```text
inner
global
```

### Declaration

`let` declares a variable in the current innermost scope only.

If the current scope already contains the same name, execution raises a runtime error:

```cd
let x = 1;
let x = 2;
```

Expected error class: runtime error.

Suggested message:

```text
IR runtime error: variable `x` already declared in this scope
```

Shadowing from an inner scope is allowed:

```cd
let x = 1;
{
  let x = 2;
  print x;
}
print x;
```

Expected output:

```text
2
1
```

### Variable Lookup

Reading a variable searches from the innermost scope outward. If no scope contains the name, execution raises the existing undefined-variable runtime error shape:

```text
IR runtime error: undefined variable `x`
```

### Assignment

Assignment searches from the innermost scope outward and updates the first matching binding.

Assignment to an outer variable:

```cd
let x = 1;
{
  x = 2;
}
print x;
```

Expected output:

```text
2
```

Assignment to a shadowed variable updates the inner binding only:

```cd
let x = 1;
{
  let x = 10;
  x = 20;
}
print x;
```

Expected output:

```text
1
```

Assignment to an undefined variable keeps the existing runtime error behavior.

### Block-Local Escape

A variable declared in a block is not visible after the block exits:

```cd
{
  let x = 1;
}
print x;
```

Expected error class: runtime error, undefined variable.

## Architecture Changes

### IR

Modify `include/IR.hpp` and `src/IR.cpp`:

- Add `IROp::BeginScope` and `IROp::EndScope`.
- Add `IRProgram::emitBeginScope()` and `IRProgram::emitEndScope()`.
- Update `irOpName`.
- Update IR printer so scope operations have stable golden output.

### IR Compiler

Modify `src/IRCompiler.cpp`:

- For `BlockStmt`, emit `BeginScope`, compile child statements, then emit `EndScope`.
- Leave top-level program statements in the initial global scope.
- Leave parser and AST shape unchanged.

### IR Interpreter

Modify `include/IRInterpreter.hpp` and `src/IRInterpreter.cpp`:

- Replace the single runtime variable map as the execution mechanism with a vector of scope maps.
- Initialize the scope stack with one global scope at the start of `execute`.
- Implement helper operations for:
  - reading the current scope
  - finding a scope containing a name from inner to outer
  - declaring in the current scope
  - assigning the nearest existing binding
  - loading the nearest existing binding
- Keep `globals()` available by returning the global scope, preserving the public API as much as possible.
- Raise a runtime error if `EndScope` would pop the global scope, because that indicates malformed IR.

## Testing Plan

Add success golden fixtures for:

1. Block shadowing:
   - inner `let` shadows outer variable
   - output proves outer variable is restored after block
2. Assignment to outer variable from inner block:
   - assignment updates nearest existing outer binding
3. Assignment to inner shadow:
   - assignment updates the inner binding and leaves outer binding unchanged

Add runtime-error golden fixtures for:

1. Duplicate declaration in the same scope.
2. Block-local variable accessed after block exit.
3. Existing assign-undefined behavior remains covered and should continue to pass.

Run:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

## Documentation Updates

Update `README.md`:

- State that blocks introduce lexical scope.
- State that same-scope duplicate `let` declarations are runtime errors.
- Keep type annotation wording unchanged except where necessary.

Update `AGENTS.md`:

- Remove the current limitation saying blocks do not introduce lexical scope.
- Record the new scope behavior.

Update `docs/roadmap.md`:

- Mark Phase 1 as implemented or note that lexical scope exists after this change.

`docs/language-grammar.ebnf` grammar does not need structural changes because block syntax is unchanged. A comment can be updated only if useful.

## Non-Goals

- Do not add a compile-time resolver in this phase.
- Do not add type checking.
- Do not add functions, closures, loops, or logical operators.
- Do not change expression grammar.
- Do not change CLI flags.

## Success Criteria

- Blocks create runtime lexical scopes.
- Same-scope duplicate declarations fail with a runtime error.
- Inner blocks can shadow outer variables.
- Variable lookup and assignment use nearest-binding lexical lookup.
- Existing assignment-to-undefined behavior remains intact.
- All golden tests and golden runner selftests pass.
