# Flow Facts Extraction Design

## Goal

Extract nullable flow-fact and narrowing logic out of `TypeChecker` into a small focused subsystem. This is an M0 front-end stabilization slice. It is intentionally behavior-preserving: existing language semantics, diagnostics, AST/IR/bytecode output, and runtime behavior must not change.

The immediate benefit is reducing `TypeChecker` complexity around nullable narrowing. The follow-on benefit is giving future nullable work, such as loop-body narrowing and post-branch narrowing, a clearer home.

## Non-Goals

This slice does not add new narrowing behavior. In particular, it does not implement:

- nullable narrowing in `while` or conditional `for` loop bodies;
- post-branch facts after terminating branches such as `if (x == nil) { return; }`;
- field, array-element, or alias-sensitive narrowing;
- parser, AST, grammar, IR, bytecode, Rust VM, or README behavior changes;
- a visitor rewrite of `TypeChecker`.

## Current Behavior to Preserve

The current type checker narrows simple nullable variables in `if` conditions:

- `if (x != nil) { ... }` narrows `x` to non-null in the then branch.
- `if (x == nil) { ... } else { ... }` narrows `x` to non-null in the else branch.
- Parenthesized conditions are unwrapped.
- Conservative logical guards combine facts:
  - `&&` combines then-branch facts from both sides.
  - `||` combines else-branch facts from both sides.
- The narrowing target is a simple resolved variable whose current binding type is `T?`; the narrowed type is `T`.
- Narrowings are scoped dynamically while checking the branch body and are removed after the branch check completes.

All existing tests and goldens should continue to pass without refreshes.

## Proposed Architecture

Add a new component:

- `include/FlowFacts.hpp`
- `src/FlowFacts.cpp`

The component owns the small vocabulary for flow facts:

- `FlowNarrowing`: a resolved binding name and the narrowed `TypeInfo`.
- `BranchFlowFacts`: then-branch and else-branch narrowing vectors.
- `FlowFacts`: helper object that computes branch facts and manages the active narrowing stack.

`TypeChecker` remains the orchestrator. It still owns scopes, bindings, module state, expression checking, and diagnostics. It delegates only the flow-fact details.

## Component Boundary

`FlowFacts` should not know about `TypeChecker::Binding`, scopes, modules, or diagnostics. Instead, it receives a lightweight resolver callback used when analyzing a variable expression:

```cpp
using VariableNarrowingResolver =
    std::function<std::optional<FlowNarrowing>(const VariableExpr&)>;
```

`TypeChecker` implements this callback by:

1. resolving the variable expression to the current binding;
2. checking whether the binding type is nullable;
3. returning the binding's resolved name and non-null type when narrowing is valid.

This keeps ownership and name-resolution rules in `TypeChecker`, while moving expression-shape analysis and fact stack management into `FlowFacts`.

## Data Flow

`TypeChecker::checkStatement` for an `if` statement will:

1. type-check the condition as it does today;
2. ask `flowFacts_.factsForIfCondition(*condition, resolver)` for branch facts;
3. check the then body inside `flowFacts_.withNarrowings(facts.thenNarrowings, ...)`;
4. check the else body inside `flowFacts_.withNarrowings(facts.elseNarrowings, ...)` when present.

`TypeChecker::variableType` will ask `flowFacts_` whether the binding's resolved name has an active narrowed type. If so, it returns that type; otherwise, it returns the binding's declared/inferred type.

The active narrowing stack keeps existing shadowing behavior by keying facts by resolved name rather than source name.

## API Sketch

The public header should stay small. A likely shape is:

```cpp
struct FlowNarrowing {
    std::string resolvedName;
    TypeInfo type;
};

struct BranchFlowFacts {
    std::vector<FlowNarrowing> thenNarrowings;
    std::vector<FlowNarrowing> elseNarrowings;
};

class FlowFacts {
public:
    using VariableNarrowingResolver =
        std::function<std::optional<FlowNarrowing>(const VariableExpr&)>;

    void clear();
    BranchFlowFacts factsForIfCondition(
        const Expr& condition,
        const VariableNarrowingResolver& resolveVariableNarrowing) const;
    std::optional<TypeInfo> narrowedTypeFor(const std::string& resolvedName) const;
    void withNarrowings(
        const std::vector<FlowNarrowing>& narrowings,
        const std::function<void()>& body);
};
```

Names may change during implementation if the final names are clearer, but the boundary should remain the same: expression fact calculation and active fact stack in `FlowFacts`, type-checker binding resolution in `TypeChecker`.

## Error Handling

This refactor should not introduce new diagnostics. Invalid or non-narrowable conditions simply produce empty branch facts, matching current behavior. Any type errors in conditions or branch bodies remain produced by existing `TypeChecker` expression and statement checks.

`FlowFacts::withNarrowings` should preserve stack state if the body throws a diagnostic. Prefer an RAII guard or equivalent exception-safe restoration so later checks cannot observe stale facts if the caller catches and continues in the future.

## Testing Strategy

Because this slice is behavior-preserving, use existing tests as the primary verification:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

No golden updates are expected. If any expected output changes, treat that as a regression unless the difference exposes a pre-existing bug and is explicitly re-scoped.

## Implementation Notes

- Move only the existing narrowing structs and helper logic first; avoid opportunistic refactors of unrelated type-checker areas.
- Keep `FlowFacts` independent of module/import symbol table work.
- Ensure `TypeChecker::check` clears the new `FlowFacts` object together with other per-run state.
- Update the CMake target to compile `src/FlowFacts.cpp`.
- Keep includes minimal to avoid increasing rebuild coupling.

## Success Criteria

- `TypeChecker` no longer owns the active narrowing vector or condition-shape narrowing algorithm.
- Existing nullable narrowing behavior is unchanged.
- Existing diagnostics and goldens are unchanged.
- The full verification command set passes.
