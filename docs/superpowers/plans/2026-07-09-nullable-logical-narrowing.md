# Nullable Logical Narrowing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend nullable narrowing so simple variables are narrowed through conservative `&&` then-branch guards and `||` else-branch guards.

**Architecture:** Keep the existing `TypeChecker::IfNarrowing` interface and extend `TypeChecker::ifNarrowing` to recursively inspect logical binary expressions. This is a front-end-only change; IR, bytecode, and runtime behavior are unchanged.

**Tech Stack:** C++17 compiler front-end, Python golden runner, markdown docs.

---

## File Structure

- Modify `src/TypeChecker.cpp`: extend `ifNarrowing` to combine narrowings from `&&` and `||` expressions.
- Modify `tests/golden/type_errors/nullable_narrowing_logical_unsupported.*`: move this behavior from unsupported type-error coverage into a success fixture.
- Create `tests/golden/nullable_logical_narrowing/input.cd`: success fixture for `&&` then-branch and `||` else-branch narrowing.
- Create `tests/golden/nullable_logical_narrowing/run.out`: runtime output for the success fixture.
- Modify `README.md`: document supported logical nullable narrowing.
- Modify `docs/roadmap.md`: mark the logical-composition slice as implemented while keeping broader dataflow future work.

---

### Task 1: Add failing success fixture

**Files:**
- Create: `tests/golden/nullable_logical_narrowing/input.cd`
- Create: `tests/golden/nullable_logical_narrowing/run.out`
- Delete after migration: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.cd`
- Delete after migration: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.err`
- Delete after migration: `tests/golden/type_errors/nullable_narrowing_logical_unsupported.exit`

- [ ] **Step 1: Create the success fixture directory**

Run:

```bash
mkdir -p tests/golden/nullable_logical_narrowing
```

- [ ] **Step 2: Write the fixture input**

Write this exact file to `tests/golden/nullable_logical_narrowing/input.cd`:

```cd
fun takesNumber(value: number): number { return value; }

let withAnd: number? = 41;
let guard = true;
if (withAnd != nil && guard) {
  print takesNumber(withAnd);
}

let withOr: number? = 42;
if (withOr == nil || false) {
  print 0;
} else {
  print takesNumber(withOr);
}
```

- [ ] **Step 3: Write the expected runtime output**

Write this exact file to `tests/golden/nullable_logical_narrowing/run.out`:

```text
41
42
```

- [ ] **Step 4: Remove the obsolete unsupported logical fixture**

Run:

```bash
rm tests/golden/type_errors/nullable_narrowing_logical_unsupported.cd \
   tests/golden/type_errors/nullable_narrowing_logical_unsupported.err \
   tests/golden/type_errors/nullable_narrowing_logical_unsupported.exit
```

- [ ] **Step 5: Verify the new fixture fails before implementation**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_logical_narrowing
```

Expected: one failing check whose stderr reports `argument 1 expects number, got number?` for `withAnd` or `withOr`.

---

### Task 2: Implement conservative logical narrowing

**Files:**
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Inspect existing direct comparison narrowing**

Run:

```bash
grep -n "TypeChecker::ifNarrowing" -A80 src/TypeChecker.cpp
```

Expected: see the current direct `== nil` / `!= nil` handling that returns `IfNarrowing` with either `thenNarrowing` or `elseNarrowing`.

- [ ] **Step 2: Add logical expression combination to `TypeChecker::ifNarrowing`**

In `src/TypeChecker.cpp`, update `TypeChecker::ifNarrowing` so it first recognizes logical binary expressions with token types `TokenType::AmpAmp` and `TokenType::PipePipe` if those are the existing token names. If the actual token names differ, use the names already used by parser/type-checker logic for `&&` and `||`.

The implementation should be equivalent to:

```cpp
if (const auto* binary = dynamic_cast<const BinaryExpr*>(&condition)) {
    if (binary->op.type == TokenType::AmpAmp) {
        const IfNarrowing left = ifNarrowing(*binary->left);
        const IfNarrowing right = ifNarrowing(*binary->right);
        IfNarrowing combined;
        combined.thenNarrowing = left.thenNarrowing ? left.thenNarrowing : right.thenNarrowing;
        return combined;
    }

    if (binary->op.type == TokenType::PipePipe) {
        const IfNarrowing left = ifNarrowing(*binary->left);
        const IfNarrowing right = ifNarrowing(*binary->right);
        IfNarrowing combined;
        combined.elseNarrowing = left.elseNarrowing ? left.elseNarrowing : right.elseNarrowing;
        return combined;
    }

    // keep existing direct nil comparison logic below
}
```

If direct comparison logic already starts with `BinaryExpr`, fold the logical cases into that same block before the comparison-operator checks.

- [ ] **Step 3: Run the focused fixture and refresh only its intended output**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable_logical_narrowing
```

Expected: the fixture passes without needing `--update` because `run.out` was written manually.

---

### Task 3: Update docs and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README nullable wording**

Find the nullable type section in `README.md` and adjust the narrowing sentence to say that direct simple-variable nil checks and conservative logical `&&` / `||` combinations are supported. The wording should explicitly say fields, indexes, loops, and post-branch flow are not narrowed.

- [ ] **Step 2: Update roadmap Phase 9 / Phase 15G notes**

In `docs/roadmap.md`, update the Phase 9 status from first-slice direct `if` nil-checks to include logical `&&` then-branch and `||` else-branch narrowing for simple variables. Leave broader flow-sensitive narrowing as future work.

- [ ] **Step 3: Run focused golden tests for nullable cases**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case nullable
```

Expected: all matching nullable golden checks pass.

---

### Task 4: Full verification and commit

**Files:**
- All files changed by Tasks 1-3

- [ ] **Step 1: Run full verification**

Run:

```bash
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

Expected: every command exits with status 0; golden, bytecode artifact, Rust VM, and cargo test counts report zero failures.

- [ ] **Step 2: Check workspace status**

Run:

```bash
git status --short
```

Expected: only intentional source, docs, and fixture changes are listed.

- [ ] **Step 3: Commit implementation**

Run:

```bash
git add src/TypeChecker.cpp README.md docs/roadmap.md \
  tests/golden/nullable_logical_narrowing \
  tests/golden/type_errors/nullable_narrowing_logical_unsupported.cd \
  tests/golden/type_errors/nullable_narrowing_logical_unsupported.err \
  tests/golden/type_errors/nullable_narrowing_logical_unsupported.exit

git commit -m "feat: narrow nullable variables through logical guards"
```

Expected: commit succeeds and includes only the implementation, docs, and fixture changes for this slice.

---

## Self-Review

- Spec coverage: The plan covers logical `&&` then-branch narrowing, logical `||` else-branch narrowing, conservative out-of-scope behavior, docs, and full verification.
- Placeholder scan: The plan contains no `TBD`, `TODO`, or unspecified test steps.
- Type consistency: The implementation references existing `TypeChecker::IfNarrowing`, `TypeChecker::ifNarrowing`, and golden runner commands already present in the project.
