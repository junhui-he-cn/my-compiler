# Simple Let Type Inference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Infer static binding types for unannotated `let` declarations from known initializer expression types, so later assignments and expression checks reject known mismatches.

**Architecture:** Reuse the existing `StaticType` enum and `TypeChecker::checkExpression()` results. The only production semantics change is that `TypeChecker::checkLetInitializer()` returns the initializer type for unannotated declarations instead of discarding it as `unknown`; tests and docs then capture the stronger static behavior and expected migration of some formerly runtime errors into type errors.

**Tech Stack:** C++17, existing recursive-descent AST/type checker, existing IR and bytecode backends, Python golden tests.

---

## File Structure

Modify:

- `src/TypeChecker.cpp` — return initializer type for unannotated `let` declarations.
- `README.md` — document simple unannotated `let` type inference and remaining inference limits.
- `docs/roadmap.md` — mark Phase 9A simple `let` inference as implemented/progress.
- `AGENTS.md` — update current language semantics for unannotated `let` bindings.

Create success fixtures:

- `tests/golden/inferred_let_assignment/input.cd`
- `tests/golden/inferred_let_assignment/run.out`
- `tests/golden/inferred_let_assignment/run_bytecode.out`
- `tests/golden/inferred_let_unknown_call_result/input.cd`
- `tests/golden/inferred_let_unknown_call_result/run.out`
- `tests/golden/inferred_let_unknown_call_result/run_bytecode.out`

Create type-error fixtures:

- `tests/golden/type_errors/inferred_let_number_assignment_mismatch.cd`
- `tests/golden/type_errors/inferred_let_number_assignment_mismatch.err`
- `tests/golden/type_errors/inferred_let_number_assignment_mismatch.exit`
- `tests/golden/type_errors/inferred_let_string_assignment_mismatch.cd`
- `tests/golden/type_errors/inferred_let_string_assignment_mismatch.err`
- `tests/golden/type_errors/inferred_let_string_assignment_mismatch.exit`
- `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.cd`
- `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.err`
- `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.exit`
- `tests/golden/type_errors/inferred_let_array_assignment_mismatch.cd`
- `tests/golden/type_errors/inferred_let_array_assignment_mismatch.err`
- `tests/golden/type_errors/inferred_let_array_assignment_mismatch.exit`
- `tests/golden/type_errors/inferred_let_function_assignment_mismatch.cd`
- `tests/golden/type_errors/inferred_let_function_assignment_mismatch.err`
- `tests/golden/type_errors/inferred_let_function_assignment_mismatch.exit`

Move existing fixtures whose behavior becomes static:

- Move `tests/golden/runtime_errors/call_non_function.cd` to `tests/golden/type_errors/call_non_function.cd`.
- Remove `tests/golden/runtime_errors/call_non_function.run.err` and `tests/golden/runtime_errors/call_non_function.exit`.
- Create `tests/golden/type_errors/call_non_function.err` and `tests/golden/type_errors/call_non_function.exit`.
- Move `tests/golden/runtime_errors/invalid_add.cd` to `tests/golden/type_errors/invalid_add.cd`.
- Remove `tests/golden/runtime_errors/invalid_add.run.err` and `tests/golden/runtime_errors/invalid_add.exit`.
- Create `tests/golden/type_errors/invalid_add.err` and `tests/golden/type_errors/invalid_add.exit`.

Do not modify:

- `include/TypeChecker.hpp` unless implementation unexpectedly needs a declaration change.
- Parser, AST, IR, bytecode, or grammar files.
- Runtime dynamic error fixtures that still depend on unknown call/index results, such as `array_dynamic_non_array` and `array_dynamic_non_number_index`.

Reference:

- `docs/superpowers/specs/2026-07-04-simple-let-type-inference-design.md`
- `src/TypeChecker.cpp::checkLetInitializer()`
- `src/TypeChecker.cpp::checkExpression()`
- `tests/golden/type_errors/typed_assignment_mismatch.*`
- `tests/golden/runtime_errors/call_non_function.*`
- `tests/golden/runtime_errors/invalid_add.*`

---

### Task 1: Add Failing Type-Error Coverage for Inferred Let Assignment Mismatches

**Files:**
- Create: `tests/golden/type_errors/inferred_let_number_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/inferred_let_number_assignment_mismatch.err`
- Create: `tests/golden/type_errors/inferred_let_number_assignment_mismatch.exit`
- Create: `tests/golden/type_errors/inferred_let_string_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/inferred_let_string_assignment_mismatch.err`
- Create: `tests/golden/type_errors/inferred_let_string_assignment_mismatch.exit`
- Create: `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.err`
- Create: `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.exit`
- Create: `tests/golden/type_errors/inferred_let_array_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/inferred_let_array_assignment_mismatch.err`
- Create: `tests/golden/type_errors/inferred_let_array_assignment_mismatch.exit`
- Create: `tests/golden/type_errors/inferred_let_function_assignment_mismatch.cd`
- Create: `tests/golden/type_errors/inferred_let_function_assignment_mismatch.err`
- Create: `tests/golden/type_errors/inferred_let_function_assignment_mismatch.exit`

- [ ] **Step 1: Add number mismatch fixture**

Create `tests/golden/type_errors/inferred_let_number_assignment_mismatch.cd`:

```cd
let x = 1;
x = "oops";
```

Create `tests/golden/type_errors/inferred_let_number_assignment_mismatch.err`:

```text
Type error at 2:1: cannot assign string to `x` of type number
```

Create `tests/golden/type_errors/inferred_let_number_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 2: Add string mismatch fixture**

Create `tests/golden/type_errors/inferred_let_string_assignment_mismatch.cd`:

```cd
let s = "hi";
s = 2;
```

Create `tests/golden/type_errors/inferred_let_string_assignment_mismatch.err`:

```text
Type error at 2:1: cannot assign number to `s` of type string
```

Create `tests/golden/type_errors/inferred_let_string_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 3: Add bool mismatch fixture**

Create `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.cd`:

```cd
let b = true;
b = nil;
```

Create `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.err`:

```text
Type error at 2:1: cannot assign nil to `b` of type bool
```

Create `tests/golden/type_errors/inferred_let_bool_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 4: Add array mismatch fixture**

Create `tests/golden/type_errors/inferred_let_array_assignment_mismatch.cd`:

```cd
let xs = [];
xs = 1;
```

Create `tests/golden/type_errors/inferred_let_array_assignment_mismatch.err`:

```text
Type error at 2:1: cannot assign number to `xs` of type array
```

Create `tests/golden/type_errors/inferred_let_array_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 5: Add function mismatch fixture**

Create `tests/golden/type_errors/inferred_let_function_assignment_mismatch.cd`:

```cd
let f = fun () {
  return 1;
};
f = 1;
```

Create `tests/golden/type_errors/inferred_let_function_assignment_mismatch.err`:

```text
Type error at 4:1: cannot assign number to `f` of type function
```

Create `tests/golden/type_errors/inferred_let_function_assignment_mismatch.exit`:

```text
1
```

- [ ] **Step 6: Run golden tests and observe expected failures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected before implementation: the new inferred-let type-error fixtures fail because the compiler currently exits successfully for these programs instead of reporting the expected type errors. Existing tests should not be edited in this task.

- [ ] **Step 7: Commit failing type-error fixtures**

Run:

```bash
git add tests/golden/type_errors/inferred_let_number_assignment_mismatch.* \
        tests/golden/type_errors/inferred_let_string_assignment_mismatch.* \
        tests/golden/type_errors/inferred_let_bool_assignment_mismatch.* \
        tests/golden/type_errors/inferred_let_array_assignment_mismatch.* \
        tests/golden/type_errors/inferred_let_function_assignment_mismatch.*
git commit -m "test: cover inferred let assignment mismatches"
```

---

### Task 2: Implement Simple Let Type Inference

**Files:**
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Change unannotated `let` initializer typing**

In `src/TypeChecker.cpp`, update `TypeChecker::checkLetInitializer()` from:

```cpp
StaticType TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return StaticType::Unknown;
    }

    const StaticType declared = resolveAnnotation(*statement.typeName);
```

to:

```cpp
StaticType TypeChecker::checkLetInitializer(const LetStmt& statement)
{
    const StaticType initializer = checkExpression(*statement.initializer);
    if (!statement.typeName) {
        return initializer;
    }

    const StaticType declared = resolveAnnotation(*statement.typeName);
```

Do not change assignment checking, annotation handling, or any parser/IR/bytecode files.

- [ ] **Step 2: Run golden tests and inspect new failures**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected after implementation:

- The five new inferred-let type-error fixtures pass.
- Existing runtime-error fixtures `call_non_function` and `invalid_add` now fail because they are reported earlier as type errors.
- No unrelated fixture should fail.

If additional existing fixtures fail, inspect each failure and only update it if the new inferred binding type intentionally changes the diagnostic phase from runtime to type checking.

- [ ] **Step 3: Commit implementation**

Run:

```bash
git add src/TypeChecker.cpp
git commit -m "feat: infer unannotated let binding types"
```

---

### Task 3: Migrate Existing Runtime Fixtures That Become Type Errors

**Files:**
- Move: `tests/golden/runtime_errors/call_non_function.cd` -> `tests/golden/type_errors/call_non_function.cd`
- Delete: `tests/golden/runtime_errors/call_non_function.run.err`
- Delete: `tests/golden/runtime_errors/call_non_function.exit`
- Create: `tests/golden/type_errors/call_non_function.err`
- Create: `tests/golden/type_errors/call_non_function.exit`
- Move: `tests/golden/runtime_errors/invalid_add.cd` -> `tests/golden/type_errors/invalid_add.cd`
- Delete: `tests/golden/runtime_errors/invalid_add.run.err`
- Delete: `tests/golden/runtime_errors/invalid_add.exit`
- Create: `tests/golden/type_errors/invalid_add.err`
- Create: `tests/golden/type_errors/invalid_add.exit`

- [ ] **Step 1: Move `call_non_function` to type errors**

Run:

```bash
git mv tests/golden/runtime_errors/call_non_function.cd tests/golden/type_errors/call_non_function.cd
rm tests/golden/runtime_errors/call_non_function.run.err tests/golden/runtime_errors/call_non_function.exit
```

Create `tests/golden/type_errors/call_non_function.err`:

```text
Type error at 2:9: can only call functions
```

Create `tests/golden/type_errors/call_non_function.exit`:

```text
1
```

- [ ] **Step 2: Move `invalid_add` to type errors**

Run:

```bash
git mv tests/golden/runtime_errors/invalid_add.cd tests/golden/type_errors/invalid_add.cd
rm tests/golden/runtime_errors/invalid_add.run.err tests/golden/runtime_errors/invalid_add.exit
```

Create `tests/golden/type_errors/invalid_add.err`:

```text
Type error at 2:9: binary `+` expects two numbers or two strings, got string and number
```

Create `tests/golden/type_errors/invalid_add.exit`:

```text
1
```

- [ ] **Step 3: Run golden tests and correct exact locations only if needed**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all type-error fixture expectations match. If the actual source columns differ, update only `tests/golden/type_errors/call_non_function.err` or `tests/golden/type_errors/invalid_add.err` to the compiler's current diagnostic location.

- [ ] **Step 4: Commit migrated fixtures**

Run:

```bash
git add tests/golden/runtime_errors/call_non_function.* \
        tests/golden/type_errors/call_non_function.* \
        tests/golden/runtime_errors/invalid_add.* \
        tests/golden/type_errors/invalid_add.*
git commit -m "test: move statically caught errors to type fixtures"
```

---

### Task 4: Add Success and Unknown-Preservation Coverage

**Files:**
- Create: `tests/golden/inferred_let_assignment/input.cd`
- Create: `tests/golden/inferred_let_assignment/run.out`
- Create: `tests/golden/inferred_let_assignment/run_bytecode.out`
- Create: `tests/golden/inferred_let_unknown_call_result/input.cd`
- Create: `tests/golden/inferred_let_unknown_call_result/run.out`
- Create: `tests/golden/inferred_let_unknown_call_result/run_bytecode.out`

- [ ] **Step 1: Add same-kind inferred assignment fixture**

Create `tests/golden/inferred_let_assignment/input.cd`:

```cd
let n = 1;
n = 2;
print n;

let s = "hi";
s = "bye";
print s;

let b = true;
b = false;
print b;

let xs = [1, "two"];
xs = [];
print xs;

let f = fun () {
  return 1;
};
f = fun () {
  return 2;
};
print f();
```

Create `tests/golden/inferred_let_assignment/run.out`:

```text
2
bye
false
[]
2
```

Create `tests/golden/inferred_let_assignment/run_bytecode.out` with the same content:

```text
2
bye
false
[]
2
```

- [ ] **Step 2: Add unknown call result preservation fixture**

Create `tests/golden/inferred_let_unknown_call_result/input.cd`:

```cd
fun id(x) {
  return x;
}

let value = id(1);
value = "later";
print value;
```

Create `tests/golden/inferred_let_unknown_call_result/run.out`:

```text
later
```

Create `tests/golden/inferred_let_unknown_call_result/run_bytecode.out` with the same content:

```text
later
```

- [ ] **Step 3: Run golden tests**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass.

- [ ] **Step 4: Commit success fixtures**

Run:

```bash
git add tests/golden/inferred_let_assignment tests/golden/inferred_let_unknown_call_result
git commit -m "test: cover inferred let success cases"
```

---

### Task 5: Update Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README type inference wording**

In `README.md`, replace this sentence in the Language section:

```markdown
Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated variables are still accepted and are not fully inferred yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.
```

with:

```markdown
Type annotations on `let` declarations are checked for the built-in annotation names `number`, `bool`, `string`, and `nil`. Unannotated `let` bindings infer known initializer types such as `number`, `bool`, `string`, `nil`, `function`, and `array`; expressions whose static type is still unknown, such as function call results, remain flexible. Function parameters, function returns, and array element types are not fully inferred yet. Blocks introduce lexical scope resolved at compile time: variables declared inside a block are not visible outside it, inner blocks may shadow outer variables, re-declaring a variable in the same scope is a type error, and reading or assigning an undefined variable is a type error.
```

- [ ] **Step 2: Update roadmap Phase 9 wording**

In `docs/roadmap.md`, replace the Phase 9 opening:

```markdown
## Phase 9: Richer Type System

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested features:
```

with:

```markdown
## Phase 9: Richer Type System

Status: in progress. Phase 9A is implemented: unannotated `let` bindings infer known initializer types and use those types for later assignment checks. Function parameter types, function return types, and array element types remain future work.

Goal: evolve the current annotation checker into a more useful static type layer.

Suggested future features:
```

Also replace this line later in Phase 9:

```markdown
Recommended first slice: infer simple unannotated `let` types and preserve those types through assignment checks without introducing new type annotation syntax.
```

with:

```markdown
Completed first slice: simple unannotated `let` types are inferred from known initializer expressions without introducing new type annotation syntax.
```

- [ ] **Step 3: Update AGENTS current semantics**

In `AGENTS.md`, replace this bullet under Current Language Semantics and Limitations:

```markdown
- `let name: type = expression;` checks explicit annotations for `number`, `bool`, `string`, and `nil`; unannotated variables are accepted without full inference.
```

with:

```markdown
- `let name: type = expression;` checks explicit annotations for `number`, `bool`, `string`, and `nil`; unannotated `let` bindings infer known initializer types, while function parameters, function returns, and array element types are not fully inferred yet.
```

- [ ] **Step 4: Verify docs mention the intended inference boundary**

Run:

```bash
grep -n "Unannotated `let` bindings infer known initializer types" README.md
grep -n "Phase 9A is implemented" docs/roadmap.md
grep -n "unannotated `let` bindings infer known initializer types" AGENTS.md
```

Expected: each command prints one matching line.

- [ ] **Step 5: Commit docs**

Run:

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document simple let type inference"
```

---

### Task 6: Full Verification and Final Review Prep

**Files:**
- Verify all changed files.

- [ ] **Step 1: Run full verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_demo
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected:

- CMake configure and build succeed.
- `ctest` reports all tests passed.
- Golden tests report all checks passed.
- Golden runner selftests report OK.

- [ ] **Step 2: Review final diff scope**

Run:

```bash
git diff --stat HEAD~5..HEAD
git status --short --branch
```

Expected:

- Changes include `src/TypeChecker.cpp`, new inferred-let fixtures, moved `call_non_function` and `invalid_add` fixtures, README, roadmap, and AGENTS.
- No parser, grammar, IR, bytecode opcode, or runtime value representation changes.
- Worktree is clean.

---

## Self-Review Checklist

- Spec coverage:
  - Unannotated `let` stores known initializer type: Task 2.
  - Explicit annotations unchanged: Task 2 relies on existing annotated branch and existing typed tests.
  - Known mismatched assignments reject: Task 1 and Task 2.
  - Unknown initializer types remain flexible: Task 4.
  - Array literals infer only `array`, not element type: Task 1 and Task 4.
  - Function expressions infer only `function`, not return type: Task 1 and Task 4.
  - Existing runtime fixtures migrated when now statically caught: Task 3.
  - Docs updated: Task 5.
- No grammar, parser, IR opcode, bytecode opcode, or runtime value representation changes are planned.
- Full verification command is included.
