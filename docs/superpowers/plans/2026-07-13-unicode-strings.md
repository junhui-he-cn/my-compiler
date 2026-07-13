# Unicode String Semantics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Change `len`, `substr`, and `charAt` from UTF-8 byte offsets to Unicode scalar-value semantics with matching C++ artifact and Rust VM coverage.

**Architecture:** Keep the existing C++ string-constant, IR, bytecode, and `.cdbc` pipeline unchanged. Centralize scalar counting and scalar-to-byte boundary conversion inside the Rust VM, then exercise the same emitted artifacts through compiler goldens, bytecode artifact tests, and Rust VM execution tests.

**Tech Stack:** C++17 compiler, text `.cdbc` artifacts, Rust 2021 VM, Python golden runners, CTest/Cargo.

---

### Task 1: Add failing scalar-semantics tests and fixtures

**Files:**
- Modify: `vm-rs/src/vm.rs` (inside the existing `#[cfg(test)] mod tests`)
- Create: `tests/golden/unicode_strings/input.cd`
- Create: `tests/bytecode_artifacts/unicode_strings/input.cd`
- Create: `tests/golden/runtime_errors/substr_unicode_start_out_of_bounds.cd`
- Create: `tests/golden/runtime_errors/substr_unicode_length_out_of_bounds.cd`
- Create: `tests/golden/runtime_errors/char_at_unicode_out_of_bounds.cd`
- Create: `tests/golden/runtime_errors/substr_unicode_negative_start.cd`
- Create: `tests/golden/runtime_errors/char_at_unicode_non_integer_index.cd`

- [ ] **Step 1: Write focused Rust tests that describe the new behavior**

Append tests next to the existing native-helper tests in `vm-rs/src/vm.rs`:

```rust
    #[test]
    fn native_string_helpers_use_unicode_scalar_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = Value::string("你🙂e\u{301}");

        let length = vm.execute_len(source.clone()).expect("len succeeds");
        assert!(matches!(length, Value::Number(value) if value == 4.0));

        let sliced = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(1.0), Value::number(2.0)],
            )
            .expect("substr succeeds");
        assert!(matches!(sliced, Value::String(value) if value == "🙂e"));

        let combined = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(2.0), Value::number(2.0)],
            )
            .expect("combining scalar slice succeeds");
        assert!(matches!(combined, Value::String(value) if value == "e\u{301}"));

        let character = vm
            .execute_native_call("charAt", vec![source, Value::number(1.0)])
            .expect("charAt succeeds");
        assert!(matches!(character, Value::String(value) if value == "🙂"));
    }

    #[test]
    fn native_string_helpers_validate_scalar_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = Value::string("你🙂");

        let empty = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(2.0), Value::number(0.0)],
            )
            .expect("empty end slice succeeds");
        assert!(matches!(empty, Value::String(value) if value.is_empty()));

        for (start, length, expected) in [
            (3.0, 0.0, "substr start offset out of bounds"),
            (1.0, 2.0, "substr length out of bounds"),
            (-1.0, 0.0, "substr start offset out of bounds"),
            (1.5, 0.0, "substr expects integer start offset"),
        ] {
            let error = vm
                .execute_native_call(
                    "substr",
                    vec![source.clone(), Value::number(start), Value::number(length)],
                )
                .expect_err("substr should fail");
            assert_eq!(error.message, expected);
        }

        for (index, expected) in [
            (2.0, "charAt index out of bounds"),
            (1.5, "charAt expects integer index"),
        ] {
            let error = vm
                .execute_native_call("charAt", vec![source.clone(), Value::number(index)])
                .expect_err("charAt should fail");
            assert_eq!(error.message, expected);
        }
    }
```

- [ ] **Step 2: Add the integration fixture source programs**

Use this exact success program in both `tests/golden/unicode_strings/input.cd`
and `tests/bytecode_artifacts/unicode_strings/input.cd`:

```text
let text = "你🙂";
print len(text);
print text.len();
print substr(text, 0, 1);
print text.substr(1, 1);
print charAt(text, 0);
print text.charAt(1);
let combined = "é";
print len(combined);
print combined.substr(0, 2);
```

Create these runtime-error sources:

```text
// tests/golden/runtime_errors/substr_unicode_start_out_of_bounds.cd
substr("你🙂", 3, 0);
```

```text
// tests/golden/runtime_errors/substr_unicode_length_out_of_bounds.cd
substr("你🙂", 1, 2);
```

```text
// tests/golden/runtime_errors/char_at_unicode_out_of_bounds.cd
charAt("你🙂", 2);
```

```text
// tests/golden/runtime_errors/substr_unicode_negative_start.cd
substr("你🙂", -1, 0);
```

```text
// tests/golden/runtime_errors/char_at_unicode_non_integer_index.cd
charAt("你🙂", 1.5);
```

- [ ] **Step 3: Run the focused tests before implementation and verify failure**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml native_string_helpers
```

Expected: the tests compile but fail because the current byte-oriented VM
reports a length of `10` for `"你🙂é"`, slices at byte offsets, or returns the
existing invalid-UTF-8 error.

### Task 2: Implement shared Unicode scalar boundaries in the Rust VM

**Files:**
- Modify: `vm-rs/src/vm.rs:868-1045` (`execute_len`, `execute_native_substr`, `execute_native_char_at`, and nearby helpers)

- [ ] **Step 1: Add a scalar boundary table helper beside `checked_integer_index`**

Add this private helper inside `impl VM`:

```rust
    fn string_scalar_offsets(text: &str) -> Vec<usize> {
        let mut offsets = text.char_indices().map(|(offset, _)| offset).collect::<Vec<_>>();
        offsets.push(text.len());
        offsets
    }
```

The returned vector has one byte offset per scalar boundary, including the
final `text.len()` sentinel. It is safe for valid Rust `String` values and
lets all operations share identical boundary logic.

- [ ] **Step 2: Change `execute_len` to count scalars for strings**

Keep the array branch and error unchanged; replace only the string branch:

```rust
            Value::String(value) => Ok(Value::number(value.chars().count() as f64)),
```

- [ ] **Step 3: Rewrite `execute_native_substr` to index the scalar table**

After validating the receiver and numeric arguments, replace byte-length and
byte-slice logic with:

```rust
        let offsets = Self::string_scalar_offsets(text);
        let scalar_count = offsets.len() - 1;
        let start = Self::checked_integer_index(
            *start_value,
            "substr expects integer start offset",
            "substr start offset out of bounds",
            scalar_count,
        )?;
        let length = Self::checked_integer_index(
            *length_value,
            "substr expects integer length",
            "substr length out of bounds",
            scalar_count,
        )?;
        if length > scalar_count - start {
            return Err(RuntimeError::new("substr length out of bounds"));
        }
        let begin = offsets[start];
        let end = offsets[start + length];
        Ok(Value::string(text[begin..end].to_string()))
```

This preserves the existing messages and permits `start == scalar_count` only
when `length == 0`.

- [ ] **Step 4: Rewrite `execute_native_char_at` to return one scalar**

Retain the empty-string check and numeric validation, then use:

```rust
        let offsets = Self::string_scalar_offsets(text);
        let scalar_count = offsets.len() - 1;
        if scalar_count == 0 {
            return Err(RuntimeError::new("charAt index out of bounds"));
        }
        let index = Self::checked_integer_index(
            *index_value,
            "charAt expects integer index",
            "charAt index out of bounds",
            scalar_count - 1,
        )?;
        let begin = offsets[index];
        let end = offsets[index + 1];
        Ok(Value::string(text[begin..end].to_string()))
```

Remove the old byte slice and `String::from_utf8` conversion. A slice between
validated `char_indices` offsets is already valid UTF-8.

- [ ] **Step 5: Run the focused Rust tests and confirm they pass**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml native_string_helpers
```

Expected: both new tests pass and no existing VM tests fail.

- [ ] **Step 6: Commit the VM implementation and unit tests**

```bash
git add vm-rs/src/vm.rs
git commit -m "feat: use unicode scalar string offsets"
```

### Task 3: Generate compiler, artifact, and runtime-error goldens

**Files:**
- Create/update: `tests/golden/unicode_strings/ast.out`
- Create/update: `tests/golden/unicode_strings/ir.out`
- Create/update: `tests/golden/unicode_strings/bytecode.out`
- Create/update: `tests/golden/unicode_strings/run.out`
- Create/update: `tests/bytecode_artifacts/unicode_strings/run.out`
- Create/update: `tests/bytecode_artifacts/unicode_strings/expected.cdbc`
- Create: `tests/golden/runtime_errors/*.run.err`
- Create: `tests/golden/runtime_errors/*.exit`

- [ ] **Step 1: Build the compiler**

Run:

```bash
cmake -S . -B build
cmake --build build
```

- [ ] **Step 2: Refresh compiler goldens for the new success fixture**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design \
  --update --update-missing --case unicode_strings
```

Expected: `ast.out`, `ir.out`, and `bytecode.out` are generated for only the
new fixture; the compiler should emit UTF-8 constants without changing the
native-call opcode shape.

- [ ] **Step 3: Produce the success run output and artifact expectation**

Emit the artifact and execute it:

```bash
tmp_artifact=$(mktemp)
./build/compiler_design --emit-bytecode "$tmp_artifact" \
  tests/golden/unicode_strings/input.cd
cargo run --quiet --manifest-path vm-rs/Cargo.toml -- run "$tmp_artifact" \
  > tests/golden/unicode_strings/run.out
cp tests/golden/unicode_strings/run.out tests/bytecode_artifacts/unicode_strings/run.out
cp "$tmp_artifact" tests/bytecode_artifacts/unicode_strings/expected.cdbc
rm -f "$tmp_artifact"
```

Expected `run.out` contents:

```text
2
2
你
🙂
你
🙂
2
é
```

- [ ] **Step 4: Refresh Unicode runtime-error expectations**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design \
  --update --case unicode_
```

Expected: each new `.run.err` keeps the existing source-location/call-stack
shape while reporting scalar-based bounds or integer validation messages, and
each `.exit` contains `1`.

- [ ] **Step 5: Run focused integration checks**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --case unicode
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case unicode
```

Expected: all new compiler, artifact, and Rust VM checks pass. Review the
generated files to ensure no unrelated fixture changed.

### Task 4: Update user and format documentation

**Files:**
- Modify: `README.md:159-176` (string and builtin semantics)
- Modify: `docs/roadmap.md:188-200` (Phase 16 status)
- Modify: `docs/bytecode-text-format.md` (value/runtime semantics note)

- [ ] **Step 1: Replace README byte-oriented wording**

Document that `len` counts Unicode scalar values, and that `substr` and
`charAt` use scalar offsets. Include the concrete examples
`len("你🙂") == 2`, `charAt("你🙂", 1) == "🙂"`, and
`substr("你🙂", 1, 1) == "🙂"`; state that grapheme segmentation and
normalization are not provided.

- [ ] **Step 2: Mark Phase 16 implemented in the roadmap**

Change the Phase 16 bullets to state that scalar-value semantics are
implemented across compiler artifacts and the Rust VM, while retaining the
non-goals for grapheme clusters, normalization, locale behavior, and regex.

- [ ] **Step 3: Clarify the `.cdbc` string contract**

Add a short note under `Value Encoding` or `native_call` in
`docs/bytecode-text-format.md`: string constants are UTF-8 text, and the VM's
string native operations interpret offsets as Unicode scalar values. Do not
change the format version or opcode list.

- [ ] **Step 4: Commit documentation and golden updates**

```bash
git add README.md docs/roadmap.md docs/bytecode-text-format.md \
  tests/golden/unicode_strings tests/golden/runtime_errors \
  tests/bytecode_artifacts/unicode_strings
git commit -m "test: cover unicode string semantics"
```

### Task 5: Run the full verification gate and finish the branch

**Files:**
- Modify: none (except removing generated `tests/__pycache__` directories)

- [ ] **Step 1: Run the complete required verification commands**

From the repository root, run:

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

Expected: every command exits zero; report the exact pass counts from CTest,
golden, artifact, Rust VM, and Cargo output.

- [ ] **Step 2: Inspect status and review the final diff**

Run:

```bash
git status --short
git diff HEAD~2..HEAD --check
git log -3 --oneline
```

Expected: only the Unicode design/plan, VM implementation, focused fixtures,
and documentation are present; no build products or `__pycache__` remain.

- [ ] **Step 3: Commit any final cleanup and report completion**

If the status check finds only intended changes, create no empty commit. If a
small generated-file correction is needed, use `git add` for the specific
paths and commit it with:

```bash
git commit -m "chore: finalize unicode string slice"
```
