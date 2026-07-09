# `typeOf(value)` Native Builtin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a shadowable native stdlib builtin `typeOf(value)` that returns the runtime type name as a string.

**Architecture:** Reuse the existing `NativeStdlib` registry and `native_call` IR/bytecode path. Type checking accepts any argument and returns `string`; C++ IR interpreter and Rust VM dispatch map runtime value variants to the same seven public type strings.

**Tech Stack:** C++17 compiler/type checker/IR interpreter, existing bytecode text artifacts, Rust VM native-call executor, Python golden tests.

---

## File Structure

- Modify `include/NativeStdlib.hpp`: add `NativeFunctionKind::TypeOf`.
- Modify `src/NativeStdlib.cpp`: register `typeOf` with arity 1.
- Modify `src/TypeChecker.cpp`: type-check unshadowed `typeOf(value)` as returning `string` and accepting any checked argument.
- Modify `include/IRInterpreter.hpp`: declare `executeNativeTypeOf`.
- Modify `src/IRInterpreter.cpp`: dispatch and execute `typeOf`.
- Modify `vm-rs/src/vm.rs`: dispatch and execute `typeOf` in Rust VM.
- Modify `tests/run_rust_vm_tests.py`: add `native_stdlib_typeof` to the Rust VM golden allowlist.
- Create `tests/golden/native_stdlib_typeof/`: success fixture with `input.cd`, `ast.out`, and `run.out` first, then generated `ir.out` and `bytecode.out`.
- Create `tests/golden/type_errors/typeof_wrong_arity.cd`, `.err`, `.exit`: native arity diagnostic.
- Create `tests/golden/type_errors/typeof_shadowed_call_non_function.cd`, `.err`, `.exit`: shadowing still uses normal call checking.
- Create `tests/bytecode_artifacts/native_stdlib_typeof/`: `.cdbc` artifact fixture and `run.out`.
- Modify `README.md`, `docs/roadmap.md`, and `AGENTS.md`: document `typeOf` and mark Phase 13C implemented.

---

### Task 1: RED success and type-error fixtures

**Files:**
- Create: `tests/golden/native_stdlib_typeof/input.cd`
- Create: `tests/golden/native_stdlib_typeof/ast.out`
- Create: `tests/golden/native_stdlib_typeof/run.out`
- Create: `tests/golden/type_errors/typeof_wrong_arity.cd`
- Create: `tests/golden/type_errors/typeof_wrong_arity.err`
- Create: `tests/golden/type_errors/typeof_wrong_arity.exit`
- Create: `tests/golden/type_errors/typeof_shadowed_call_non_function.cd`
- Create: `tests/golden/type_errors/typeof_shadowed_call_non_function.err`
- Create: `tests/golden/type_errors/typeof_shadowed_call_non_function.exit`

- [ ] **Step 1: Add success fixture input**

Create `tests/golden/native_stdlib_typeof/input.cd`:

```cd
print typeOf(nil);
print typeOf(1);
print typeOf(true);
print typeOf("x");
print typeOf(fun () { return nil; });
print typeOf([1]);
print typeOf({ x: 1 });
struct Box { value: number }
print typeOf(Box { value: 1 });
let typeOf = fun (value) { return "shadowed"; };
print typeOf(123);
```

- [ ] **Step 2: Add expected AST**

Create `tests/golden/native_stdlib_typeof/ast.out`:

```text
Program
  Print (call typeOf nil)
  Print (call typeOf 1)
  Print (call typeOf true)
  Print (call typeOf "x")
  Print (call typeOf (fun () (return nil)))
  Print (call typeOf (array 1))
  Print (call typeOf (struct x: 1))
  Struct Box
    field value: number
  Print (call typeOf (construct Box value: 1))
  Let typeOf = (fun (value) (return "shadowed"))
  Print (call typeOf 123)
```

- [ ] **Step 3: Add expected runtime output**

Create `tests/golden/native_stdlib_typeof/run.out`:

```text
nil
number
bool
string
function
array
struct
struct
shadowed
```

- [ ] **Step 4: Add wrong-arity type-error fixture**

Create `tests/golden/type_errors/typeof_wrong_arity.cd`:

```cd
typeOf();
```

Create `tests/golden/type_errors/typeof_wrong_arity.err`:

```text
Type error at 1:7: expected 1 arguments but got 0
```

Create `tests/golden/type_errors/typeof_wrong_arity.exit`:

```text
1
```

- [ ] **Step 5: Add shadowed non-function type-error fixture**

Create `tests/golden/type_errors/typeof_shadowed_call_non_function.cd`:

```cd
let typeOf = 123;
typeOf(nil);
```

Create `tests/golden/type_errors/typeof_shadowed_call_non_function.err`:

```text
Type error at 2:7: can only call functions
```

Create `tests/golden/type_errors/typeof_shadowed_call_non_function.exit`:

```text
1
```

- [ ] **Step 6: Verify RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_typeof` and `typeof_wrong_arity` fail because unshadowed `typeOf` is not registered yet. `typeof_shadowed_call_non_function` should already pass because shadowed names use ordinary call checking.

- [ ] **Step 7: Commit RED fixtures**

```bash
git add tests/golden/native_stdlib_typeof tests/golden/type_errors/typeof_*
git commit -m "test: add typeof builtin fixtures"
```

---

### Task 2: Register and type-check `typeOf`

**Files:**
- Modify: `include/NativeStdlib.hpp`
- Modify: `src/NativeStdlib.cpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add native function kind**

In `include/NativeStdlib.hpp`, add `TypeOf` after `CharAt`:

```cpp
enum class NativeFunctionKind {
    Push,
    Pop,
    Floor,
    Ceil,
    Sqrt,
    Str,
    Substr,
    CharAt,
    TypeOf,
};
```

- [ ] **Step 2: Register `typeOf`**

In `src/NativeStdlib.cpp`, increase the array size and add `typeOf` after `charAt`:

```cpp
constexpr std::array<NativeFunctionSignature, 9> kNativeFunctions{{
    {"push", 2, NativeFunctionKind::Push},
    {"pop", 1, NativeFunctionKind::Pop},
    {"floor", 1, NativeFunctionKind::Floor},
    {"ceil", 1, NativeFunctionKind::Ceil},
    {"sqrt", 1, NativeFunctionKind::Sqrt},
    {"str", 1, NativeFunctionKind::Str},
    {"substr", 3, NativeFunctionKind::Substr},
    {"charAt", 2, NativeFunctionKind::CharAt},
    {"typeOf", 1, NativeFunctionKind::TypeOf},
}};
```

- [ ] **Step 3: Type-check `typeOf` as returning string**

In `src/TypeChecker.cpp`, inside `TypeChecker::checkNativeStdlibCall`, add this branch after `NativeFunctionKind::CharAt`:

```cpp
    case NativeFunctionKind::TypeOf:
        checkExpressionInfo(*expression.arguments[0]);
        return CheckedExpression{simpleType(StaticType::String)};
```

The existing arity check at the top of `checkNativeStdlibCall` should remain responsible for `typeOf()` and `typeOf(a, b)` errors.

- [ ] **Step 4: Verify type-checking GREEN and runtime still RED**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: type-error fixtures pass. `native_stdlib_typeof` parse/AST/type-checks, but its `--run` mode fails with runtime error `unknown native stdlib function \`typeOf\`` because interpreter dispatch is not implemented yet.

- [ ] **Step 5: Commit registry and type checker**

```bash
git add include/NativeStdlib.hpp src/NativeStdlib.cpp src/TypeChecker.cpp
git commit -m "feat: type check typeof builtin"
```

---

### Task 3: Execute `typeOf` in C++ IR interpreter and generate CLI goldens

**Files:**
- Modify: `include/IRInterpreter.hpp`
- Modify: `src/IRInterpreter.cpp`
- Modify/Create: `tests/golden/native_stdlib_typeof/ir.out`
- Modify/Create: `tests/golden/native_stdlib_typeof/bytecode.out`

- [ ] **Step 1: Declare C++ native executor**

In `include/IRInterpreter.hpp`, add this declaration after `executeNativeCharAt`:

```cpp
    Value executeNativeTypeOf(const Frame& frame, const std::vector<IRRegister>& arguments);
```

- [ ] **Step 2: Dispatch `typeOf` in C++ runtime**

In `src/IRInterpreter.cpp`, inside `IRInterpreter::executeNativeCall`, add this after the `charAt` branch:

```cpp
    if (name == "typeOf") {
        return executeNativeTypeOf(frame, arguments);
    }
```

- [ ] **Step 3: Implement C++ runtime behavior**

In `src/IRInterpreter.cpp`, add this method after `executeNativeCharAt`:

```cpp
Value IRInterpreter::executeNativeTypeOf(const Frame& frame, const std::vector<IRRegister>& arguments)
{
    if (arguments.size() != 1) {
        throw IRRuntimeError("typeOf expects 1 arguments");
    }
    return Value::string(typeName(readRegister(frame, arguments[0]).type()));
}
```

This reuses the existing anonymous `typeName(Value::Type)` helper, which already returns `nil`, `number`, `bool`, `string`, `function`, `array`, and `struct`.

- [ ] **Step 4: Verify C++ runtime GREEN**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: `native_stdlib_typeof` run output passes. The fixture may still be missing generated `ir.out` and `bytecode.out`, which are added in the next step.

- [ ] **Step 5: Generate IR and bytecode goldens**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
git diff -- tests/golden/native_stdlib_typeof
```

Expected: `tests/golden/native_stdlib_typeof/ir.out` and `tests/golden/native_stdlib_typeof/bytecode.out` are created. Review that unshadowed calls contain `native_call ... typeOf(...)`, while the shadowed final call uses normal function call lowering.

If `--update` modifies unrelated existing goldens, stop and inspect `git diff`
before committing. Do not commit unrelated golden changes with this slice.

- [ ] **Step 6: Commit C++ runtime and generated CLI goldens**

```bash
git add include/IRInterpreter.hpp src/IRInterpreter.cpp tests/golden/native_stdlib_typeof
git commit -m "feat: execute typeof builtin"
```

---

### Task 4: Add Rust VM parity and bytecode artifact coverage

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Modify: `tests/run_rust_vm_tests.py`
- Create: `tests/bytecode_artifacts/native_stdlib_typeof/input.cd`
- Create: `tests/bytecode_artifacts/native_stdlib_typeof/run.out`
- Create: `tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc`

- [ ] **Step 1: Dispatch `typeOf` in Rust VM**

In `vm-rs/src/vm.rs`, inside `execute_native_call`, add this match arm after `"charAt"`:

```rust
            "typeOf" => self.execute_native_type_of(arguments),
```

- [ ] **Step 2: Implement Rust runtime behavior**

In `vm-rs/src/vm.rs`, add this method after `execute_native_char_at`:

```rust
    fn execute_native_type_of(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("typeOf expects 1 arguments"));
        }
        Ok(Value::string(arguments[0].type_name()))
    }
```

- [ ] **Step 3: Add Rust VM golden allowlist entry**

In `tests/run_rust_vm_tests.py`, add `"native_stdlib_typeof",` near the other native stdlib golden fixtures:

```python
            "native_stdlib_math",
            "native_stdlib_push_pop",
            "native_stdlib_strings",
            "native_stdlib_typeof",
```

- [ ] **Step 4: Add bytecode artifact input**

Create `tests/bytecode_artifacts/native_stdlib_typeof/input.cd`:

```cd
print typeOf(nil);
print typeOf(1);
print typeOf([1]);
print typeOf({ x: 1 });
```

- [ ] **Step 5: Add bytecode artifact expected run output**

Create `tests/bytecode_artifacts/native_stdlib_typeof/run.out`:

```text
nil
number
array
struct
```

- [ ] **Step 6: Generate `.cdbc` artifact**

Run:

```bash
./build/compiler_design --emit-bytecode tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc tests/bytecode_artifacts/native_stdlib_typeof/input.cd
sed -n '1,160p' tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc
```

Expected: `expected.cdbc` contains name entries for `typeOf` and `native_call` instructions that reference those names.

- [ ] **Step 7: Verify Rust VM parity GREEN**

Run:

```bash
cmake --build build
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case native_stdlib_typeof
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: bytecode artifact tests pass, the `native_stdlib_typeof` Rust VM golden passes, and Rust unit tests pass.

- [ ] **Step 8: Commit Rust VM and bytecode artifact slice**

```bash
git add vm-rs/src/vm.rs tests/run_rust_vm_tests.py tests/bytecode_artifacts/native_stdlib_typeof
git commit -m "feat: add typeof rust vm parity"
```

---

### Task 5: Documentation and final verification

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README builtin list**

In `README.md`, update the top language summary builtin sentence to include `typeOf`:

```markdown
source imports, and builtins such as `len`, `push`, `pop`, `floor`, `ceil`,
`sqrt`, `str`, `substr`, `charAt`, and `typeOf`.
```

- [ ] **Step 2: Add README `typeOf` paragraph**

In `README.md`, after the string native stdlib paragraph, add:

```markdown
The debug native stdlib function `typeOf(value)` returns the current runtime type name as a string: `"nil"`, `"number"`, `"bool"`, `"string"`, `"function"`, `"array"`, or `"struct"`. Named struct values return `"struct"`, and arrays return `"array"` regardless of static element type. A user binding named `typeOf` shadows the builtin.
```

- [ ] **Step 3: Update roadmap Phase 13 status**

In `docs/roadmap.md`, update Phase 13 status to mention Phase 13C:

```markdown
Status: in progress. Phase 13A is implemented: `floor(number)`, `ceil(number)`, and `sqrt(number)` are shadowable native stdlib functions using the generic `native_call` path. A string helper slice is implemented with `str(value)`, `substr(string, start, length)`, and `charAt(string, index)` on the same shadowable native-call path. Phase 13C is implemented: `typeOf(value)` returns runtime type names as strings on the shadowable native-call path. `len` remains supported through its legacy dedicated IR/bytecode opcode and still awaits migration if a unified builtin path becomes valuable.
```

In the Phase 13 suggested builtins list, change the debug helper bullet to:

```markdown
- Debug helper: `typeOf`. Implemented for runtime type names (`nil`, `number`, `bool`, `string`, `function`, `array`, and `struct`).
```

In the near-term queue and recommendation sections, remove Phase 13C as the next recommendation and make Phase 9G the first recommendation.

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, update the native stdlib semantics near the numeric/string stdlib bullets by adding:

```markdown
- The debug native stdlib includes shadowable `typeOf(value)`, which returns `"nil"`, `"number"`, `"bool"`, `"string"`, `"function"`, `"array"`, or `"struct"` based on the runtime value. Named struct values return `"struct"`, and arrays return `"array"` regardless of static element type.
```

- [ ] **Step 5: Run full verification**

Run exactly:

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
git status --short
```

Expected:

- CMake configure and build exit 0.
- CTest reports all tests passed.
- Golden tests report 0 failed.
- Golden selftests report OK.
- Bytecode artifact tests report 0 failed.
- Rust VM golden tests report 0 failed.
- Cargo tests report all tests passed.
- `git status --short` shows only intentional documentation changes before the final commit.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document typeof builtin"
```

- [ ] **Step 7: Run fresh final verification after docs commit**

Run exactly:

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
git status --short
```

Expected: all commands exit 0 and `git status --short` is clean.

- [ ] **Step 8: Report results**

Report:

- All commits created during the `typeOf` phase.
- Exact verification commands and pass/fail counts.
- Final `git status --short` result.
