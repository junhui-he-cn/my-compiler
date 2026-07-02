# Closures Design

Date: 2026-07-02

## Goal

Implement Phase 6B closures for the existing `fun` declaration syntax. Nested function declarations may capture variables from enclosing local scopes. Captures use reference semantics so reads and assignments share the same runtime storage cell.

This phase does not add lambda or function-expression syntax. Function expressions/lambdas should be added to the roadmap as a later Phase 6C after closure semantics are stable.

## Scope

In scope:

- Nested `fun` declarations capture outer local variables.
- Captured variables are shared by reference.
- Captured variables can be read and assigned inside closures.
- Multiple closures that capture the same variable observe the same updates.
- Returned functions keep captured variables alive after the defining function returns.
- Existing recursion, global access, arity checks, `return`, bare `return;`, and implicit `nil` returns continue to work.
- The existing `function_capture_local` type-error behavior changes: local capture becomes valid.
- Documentation updates include `README.md`, `docs/language-grammar.ebnf` if wording needs clarification, `docs/roadmap.md`, and `AGENTS.md`.

Out of scope:

- Lambda/function-expression syntax.
- Function type annotations.
- Return type annotations or return type checking.
- Capture-list syntax.
- Optimizing captures into explicit IR capture tables.

## Language Semantics

Closures are created by existing function declarations:

```text
fun makeCounter() {
  let count = 0;
  fun inc() {
    count = count + 1;
    return count;
  }
  return inc;
}

let c = makeCounter();
print c();
print c();
```

Expected output:

```text
1
2
```

A function value captures the variables visible at the point where the function declaration executes. Captures are by reference: the closure stores references to variable cells, not copies of values. Assignment updates the cell, and all closures sharing that cell observe the update.

Name resolution remains lexical and compile-time. If an inner scope declares `let x`, it shadows any outer `x`; reads and assignments resolve to the nearest binding, as they do today.

Global variables remain available to functions. Globals can be read and assigned by functions and closures, preserving current Phase 6A behavior.

## Resolver and Type Checker

The current type checker rejects references to non-global bindings from inside a function body. This restriction should be removed.

The resolver continues to assign each binding a unique resolved name such as `count#1`. The same resolved-name maps continue to drive IR lowering:

- `let` declarations map to resolved local names.
- Function names map to resolved function-binding names.
- Parameters map to resolved parameter names.
- Variable reads and assignments map to the nearest lexical binding.

The type checker still reports existing diagnostics for:

- Undefined variables.
- Duplicate same-scope declarations.
- Wrong function arity when statically known.
- `return` outside a function.
- Non-function calls when statically known.
- Existing operator type errors.

Captured local reads and assignments are valid and should not emit `cannot capture local variable` diagnostics.

## Runtime Environment Model

Introduce a runtime cell abstraction:

```cpp
struct Cell {
    Value value;
};
```

Runtime environments store `std::shared_ptr<Cell>` rather than raw `Value` objects. Updating a variable mutates `cell->value`. This gives captured variables reference semantics and lets captured locals outlive their defining frame.

Recommended frame shape:

```cpp
struct Frame {
    std::vector<Value> registers;
    std::unordered_map<std::string, std::shared_ptr<Cell>> locals;
    std::unordered_map<std::string, std::shared_ptr<Cell>> closure;
};
```

The interpreter keeps globals as cells too:

```cpp
std::unordered_map<std::string, std::shared_ptr<Cell>> globals_;
```

Variable lookup order during function execution:

1. Current frame locals.
2. Function value closure environment.
3. Globals.

At top level, stores populate globals. In a function call, parameters and `let` declarations create new local cells. Assignment finds the resolved name using the same lookup order and mutates the matching cell.

## Function Values

Extend `FunctionValue` with a captured environment:

```cpp
struct FunctionValue {
    std::string name;
    std::size_t functionIndex = 0;
    std::size_t arity = 0;
    std::shared_ptr<Environment> closure;
};
```

`Environment` can be an alias or small struct around a `std::unordered_map<std::string, std::shared_ptr<Cell>>`.

When `make_function` executes, the interpreter creates a function value whose closure environment contains the currently visible local cells and closure cells. Globals do not need to be copied into the closure because global lookup remains separate and dynamic.

When a function is called, the interpreter creates a fresh frame with:

- Registers sized to the IR function register count.
- Parameter cells created from argument values.
- The function value's closure environment attached to the frame.

This allows returned functions to keep captured locals alive through shared ownership of cells.

Function declarations inside local scopes need a self-binding cell before the function value captures its environment. This is required for nested recursive closures. One acceptable lowering is:

1. Store a placeholder `nil` cell for the function's resolved name in the enclosing scope.
2. Execute `make_function`, which captures the enclosing scope including that placeholder cell.
3. Assign the resulting function value into the existing placeholder cell.

Top-level function declarations may continue to work through globals, but using the same placeholder-and-assign pattern everywhere is also valid if it preserves existing IR and runtime behavior.

## IR Strategy

No new IR operation is required for the first closure implementation. The existing `make_function` operation becomes responsible for capturing the current runtime environment when executed.

The IR function table remains the same:

- Function bodies still compile into separate `IRFunction` entries.
- Function declarations may emit a placeholder store followed by `make_function` and assignment when needed for nested self-recursion.
- Calls still use `call` with argument registers.
- Returns still use `return`.

Explicit capture metadata is intentionally deferred. It can be added later if the interpreter needs smaller closure environments or if IR printing should show capture lists.

## Tests

Add or update golden fixtures for:

1. Reading a captured variable after its defining function returns.
2. Counter-style mutation of a captured variable.
3. Multiple closures sharing the same captured variable cell.
4. Shadowing: inner `let` bindings do not accidentally update outer captured variables.
5. Captured assignment: assigning to an outer local from inside a nested function updates the outer cell.
6. Migration of the existing `function_capture_local` type-error fixture into a successful closure fixture.
7. Regression of Phase 6A behavior: recursion, globals, arity diagnostics, `return`, and call-non-function runtime errors.

Expected full verification:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

## Documentation

Update user-facing and project-memory docs after implementation:

- `README.md`: document closure behavior, reference capture, and the absence of lambda syntax.
- `docs/roadmap.md`: mark Phase 6B closures as implemented when complete and add Phase 6C for function expressions/lambdas.
- `AGENTS.md`: record the closure runtime convention: variables are stored in shared cells and closures capture local cells by reference.

Suggested roadmap addition:

```markdown
### Phase 6C: Function Expressions / Lambdas

Future work. Add expression-level function literals after closure semantics are stable. This phase should reuse the same by-reference closure capture model introduced in Phase 6B.
```

## Risks and Mitigations

- **Capturing too much environment:** capturing all visible local cells is simple but may retain more cells than necessary. This is acceptable for the demo compiler; explicit capture analysis can optimize later.
- **Global shadowing bugs:** lookup must prefer locals and closure cells over globals to preserve lexical resolution.
- **Assignment semantics:** assignment must mutate an existing cell, not replace a map entry, or shared closure updates will break.
- **Function equality:** existing function equality by function index may treat different closure instances as equal. This should be reviewed during implementation. If needed, assign each function value a unique identity or compare closure identity as well.
- **Recursive closures:** nested recursive functions should still work because the function declaration stores its function value in a cell visible to its own body when appropriate.

## Approval Status

Approved design choices:

- Captures are by reference.
- Scope is limited to existing nested `fun` declarations.
- Lambda/function-expression syntax is deferred to a new roadmap Phase 6C.
- Runtime uses shared variable cells to keep captured locals alive.
