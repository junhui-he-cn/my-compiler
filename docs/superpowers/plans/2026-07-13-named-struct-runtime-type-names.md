# Named Struct Runtime Type Names Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make named struct runtime values report their static struct name through `typeOf`, while anonymous structs still report `struct`.

**Architecture:** Add optional struct type-name metadata to IR/bytecode struct construction, `.cdbc` text artifacts, and Rust VM runtime struct values. Named struct constructors emit the metadata; anonymous struct literals omit it; `typeOf` reads the metadata without changing struct printing, equality, field access, field assignment, or static type checking.

**Tech Stack:** C++17 compiler IR/bytecode pipeline, `.cdbc` text artifacts, Rust VM parser/formatter/executor, Python golden and artifact tests, Cargo unit tests.

---

## File Structure

Create:

- `tests/golden/struct_runtime_type_name/input.cd` — named vs anonymous `typeOf` with aliasing and field mutation.
- `tests/golden/struct_runtime_type_name/run.out` — expected runtime output.
- `tests/golden/struct_runtime_type_name_namespace/input.cd` — namespaced struct constructor `typeOf` fixture.
- `tests/golden/struct_runtime_type_name_namespace/geometry.cd` — exported namespaced struct type.
- `tests/golden/struct_runtime_type_name_namespace/run.out` — expected qualified runtime type name.
- `tests/bytecode_artifacts/named_struct_runtime_type_name/input.cd` — artifact fixture for named struct construction.
- `tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc` — expected `.cdbc` named struct form.
- `tests/bytecode_artifacts/named_struct_runtime_type_name/run.out` — expected VM output.

Modify:

- `include/Value.hpp` — add optional struct type-name metadata to C++ `StructValue`.
- `include/IR.hpp` — add optional struct type-name operand and overload/extend `emitStruct`.
- `src/IR.cpp` — print named struct IR and store optional type-name operand.
- `include/IRCompiler.hpp` — adjust struct emission helper signature.
- `src/IRCompiler.cpp` — emit named struct type names for `StructConstructExpr`.
- `include/Bytecode.hpp` — add optional struct type-name operand.
- `src/Bytecode.cpp` — print named struct debug bytecode.
- `src/BytecodeCompiler.cpp` — lower optional IR type-name operand to bytecode.
- `src/BytecodeTextEmitter.cpp` — emit anonymous and named `.cdbc` struct forms.
- `docs/bytecode-text-format.md` — document optional struct type-name prefix.
- `vm-rs/src/bytecode.rs` — add `type_name: Option<usize>` to `Instruction::Struct`.
- `vm-rs/src/format.rs` — parse/format `struct {..}` and `struct nType {..}` and update unit tests.
- `vm-rs/src/runtime.rs` — add optional type name to `StructValue`.
- `vm-rs/src/value.rs` — make `Value::type_name()` return named struct metadata when present.
- `vm-rs/src/vm.rs` — construct struct values with optional type-name metadata.
- `tests/golden/native_stdlib_typeof/run.out` — update named `Box` case from `struct` to `Box`.
- `tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc` — update named `Box` struct instruction to named form.
- `README.md` — document named struct `typeOf` behavior.
- `docs/roadmap.md` — mark named runtime type-name decision implemented.

Do not modify:

- `docs/language-grammar.ebnf` — syntax is unchanged.
- Parser or type checker behavior — constructor validation and static type names already exist.
- Struct print format, equality semantics, field access, field assignment, or methods.

---

### Task 1: Add failing runtime and artifact coverage

**Files:**
- Create: `tests/golden/struct_runtime_type_name/input.cd`
- Create: `tests/golden/struct_runtime_type_name/run.out`
- Create: `tests/golden/struct_runtime_type_name_namespace/input.cd`
- Create: `tests/golden/struct_runtime_type_name_namespace/geometry.cd`
- Create: `tests/golden/struct_runtime_type_name_namespace/run.out`
- Create: `tests/bytecode_artifacts/named_struct_runtime_type_name/input.cd`
- Create: `tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc`
- Create: `tests/bytecode_artifacts/named_struct_runtime_type_name/run.out`
- Modify: `tests/golden/native_stdlib_typeof/run.out`

- [ ] **Step 1: Add named vs anonymous runtime fixture**

```sh
mkdir -p tests/golden/struct_runtime_type_name
cat > tests/golden/struct_runtime_type_name/input.cd <<'CASE'
struct Person { name: string }
let p = Person { name: "Ada" };
let q = p;
q.name = "Grace";
print typeOf(p);
print p.name;
print typeOf({ name: "Ada" });
CASE
cat > tests/golden/struct_runtime_type_name/run.out <<'CASE'
Person
Grace
struct
CASE
```

- [ ] **Step 2: Add namespaced runtime fixture**

```sh
mkdir -p tests/golden/struct_runtime_type_name_namespace
cat > tests/golden/struct_runtime_type_name_namespace/geometry.cd <<'CASE'
struct Point { x: number, y: number }
export Point;
CASE
cat > tests/golden/struct_runtime_type_name_namespace/input.cd <<'CASE'
import "./geometry.cd" as geo;
print typeOf(geo.Point { x: 1, y: 2 });
CASE
cat > tests/golden/struct_runtime_type_name_namespace/run.out <<'CASE'
geo.Point
CASE
```

- [ ] **Step 3: Update existing `typeOf` expected output**

Replace `tests/golden/native_stdlib_typeof/run.out` with:

```sh
cat > tests/golden/native_stdlib_typeof/run.out <<'CASE'
nil
number
bool
string
function
array
struct
Box
shadowed
CASE
```

- [ ] **Step 4: Add bytecode artifact fixture for named struct form**

```sh
mkdir -p tests/bytecode_artifacts/named_struct_runtime_type_name
cat > tests/bytecode_artifacts/named_struct_runtime_type_name/input.cd <<'CASE'
struct Box { value: number }
print typeOf(Box { value: 1 });
CASE
cat > tests/bytecode_artifacts/named_struct_runtime_type_name/run.out <<'CASE'
Box
CASE
cat > tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc <<'CASE'
cdbc 0.1

constants:
  c0 = number 1

names:
  n0 = "Box"
  n1 = "value"
  n2 = "typeOf"

main registers=3:
  r0 = constant c0
  r1 = struct n0 {n1: r0}
  r2 = native_call n2 [r1]
  print r2
CASE
```

- [ ] **Step 5: Run focused RED tests**

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case struct_runtime_type_name \
  --case struct_runtime_type_name_namespace \
  --case native_stdlib_typeof
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: Rust VM golden tests fail because named structs still report `struct`. Bytecode artifact tests fail for `named_struct_runtime_type_name` and `native_stdlib_typeof` because emitted `.cdbc` still uses anonymous `struct {..}` form.

- [ ] **Step 6: Commit failing coverage**

```sh
git add \
  tests/golden/struct_runtime_type_name \
  tests/golden/struct_runtime_type_name_namespace \
  tests/golden/native_stdlib_typeof/run.out \
  tests/bytecode_artifacts/named_struct_runtime_type_name
git commit -m "test: add named struct runtime type coverage"
```

---

### Task 2: Carry optional type names through C++ IR and bytecode

**Files:**
- Modify: `include/Value.hpp`
- Modify: `include/IR.hpp`
- Modify: `src/IR.cpp`
- Modify: `include/IRCompiler.hpp`
- Modify: `src/IRCompiler.cpp`
- Modify: `include/Bytecode.hpp`
- Modify: `src/Bytecode.cpp`
- Modify: `src/BytecodeCompiler.cpp`
- Modify: `src/BytecodeTextEmitter.cpp`
- Modify: `docs/bytecode-text-format.md`

- [ ] **Step 1: Add optional type-name metadata to C++ struct values and instructions**

In `include/Value.hpp`, add this include with the other standard includes:

```cpp
#include <optional>
```

In `include/Value.hpp`, add this field to `struct StructValue` before `fields`:

```cpp
    std::optional<std::string> typeName;
```

In `include/IR.hpp`, append this member to `struct IRInstruction` after `std::vector<std::size_t> operands;`:

```cpp
    std::optional<std::size_t> typeNameOperand;
```

Change the `emitStruct` declaration from:

```cpp
    IRRegister emitStruct(std::vector<std::size_t> fieldNames, std::vector<IRRegister> fieldValues);
```

to:

```cpp
    IRRegister emitStruct(
        std::vector<std::size_t> fieldNames,
        std::vector<IRRegister> fieldValues,
        std::optional<std::size_t> typeNameOperand = std::nullopt);
```

In `include/Bytecode.hpp`, append this member to `struct BytecodeInstruction` after `std::vector<std::uint32_t> operands;`:

```cpp
    std::optional<std::uint32_t> typeNameOperand;
```

- [ ] **Step 2: Update IR struct emission and printing**

In `src/IR.cpp`, change `IRProgram::emitStruct` to:

```cpp
IRRegister IRProgram::emitStruct(
    std::vector<std::size_t> fieldNames,
    std::vector<IRRegister> fieldValues,
    std::optional<std::size_t> typeNameOperand)
{
    IRRegister dest = makeRegister();
    IRInstruction instruction{IROp::Struct, dest, std::nullopt, std::nullopt, std::move(fieldValues), 0};
    instruction.operands = std::move(fieldNames);
    instruction.typeNameOperand = typeNameOperand;
    emit(std::move(instruction));
    return dest;
}
```

In `printInstruction(...)`, inside the `IROp::Struct` branch and before `out << " {";`, add:

```cpp
        if (instruction.typeNameOperand) {
            out << " ";
            if (*instruction.typeNameOperand < program.names().size()) {
                out << program.names()[*instruction.typeNameOperand];
            } else {
                out << "@" << *instruction.typeNameOperand;
            }
        }
```

- [ ] **Step 3: Update IRCompiler struct helpers**

In `include/IRCompiler.hpp`, change:

```cpp
    IRRegister emitStructFields(const std::vector<StructField>& fields);
```

to:

```cpp
    IRRegister emitStructFields(const std::vector<StructField>& fields, std::optional<std::string> typeName = std::nullopt);
```

In `src/IRCompiler.cpp`, change `emitStructFields` to:

```cpp
IRRegister IRCompiler::emitStructFields(const std::vector<StructField>& fields, std::optional<std::string> typeName)
{
    std::vector<std::size_t> names;
    std::vector<IRRegister> values;
    names.reserve(fields.size());
    values.reserve(fields.size());

    std::optional<std::size_t> typeNameOperand;
    if (typeName) {
        typeNameOperand = ir_.addName(std::move(*typeName));
    }

    for (const StructField& field : fields) {
        names.push_back(ir_.addName(field.name.lexeme));
        values.push_back(compileExpression(*field.value));
    }
    return ir_.emitStruct(std::move(names), std::move(values), typeNameOperand);
}
```

Keep `emitStruct` as:

```cpp
IRRegister IRCompiler::emitStruct(const StructExpr& expression)
{
    return emitStructFields(expression.fields);
}
```

Change `emitStructConstructor` to:

```cpp
IRRegister IRCompiler::emitStructConstructor(const StructConstructExpr& expression)
{
    std::string typeName = expression.name.lexeme;
    if (expression.qualifier) {
        typeName = expression.qualifier->lexeme + "." + typeName;
    }
    return emitStructFields(expression.fields, std::move(typeName));
}
```

- [ ] **Step 4: Lower optional IR type name into bytecode**

In `src/BytecodeCompiler.cpp`, add this helper after `lowerOperands(...)`:

```cpp
std::optional<std::uint32_t> lowerOperand(std::optional<std::size_t> operand)
{
    if (!operand) {
        return std::nullopt;
    }
    return checkedU32(*operand, "operand out of range");
}
```

In `BytecodeCompiler::lowerInstruction`, append `lowerOperand(instruction.typeNameOperand)` to the aggregate return:

```cpp
    return BytecodeInstruction{
        lowerOp(instruction.op),
        lowerRegister(instruction.dest),
        lowerRegister(instruction.left),
        lowerRegister(instruction.right),
        lowerRegisters(instruction.arguments),
        checkedU32(instruction.operand, "operand out of range"),
        lowerOperands(instruction.operands),
        lowerOperand(instruction.typeNameOperand)};
```

- [ ] **Step 5: Update bytecode debug printing**

In `src/Bytecode.cpp`, inside the `BytecodeOp::Struct` branch and before `out << " {";`, add:

```cpp
        if (instruction.typeNameOperand) {
            printNameOperand(out, program, *instruction.typeNameOperand);
        }
```

This makes `--bytecode` print `struct @N TypeName { ... }` for named constructors and preserves the existing anonymous form.

- [ ] **Step 6: Update `.cdbc` text emitter**

In `src/BytecodeTextEmitter.cpp`, inside `writeInstruction(...)` for `BytecodeOp::Struct`, replace:

```cpp
        out << reg(requireDest(instruction)) << " = struct {";
```

with:

```cpp
        out << reg(requireDest(instruction)) << " = struct";
        if (instruction.typeNameOperand) {
            out << ' ' << nameRef(*instruction.typeNameOperand);
        }
        out << " {";
```

- [ ] **Step 7: Update bytecode text format docs**

In `docs/bytecode-text-format.md`, replace the struct shape block:

```text
rD = struct {nName: rValue, ...}
```

with:

```text
rD = struct {nName: rValue, ...}
rD = struct nType {nName: rValue, ...}
```

After that block, add:

```markdown
The optional `nType` name-table reference records a named struct runtime type name for `typeOf`. Anonymous struct literals omit it and continue to report `"struct"`.
```

- [ ] **Step 8: Build and inspect named struct `.cdbc` RED/GREEN boundary**

```sh
cmake --build build
./build/compiler_design --emit-bytecode /tmp/named-struct-runtime-type-name.cdbc tests/bytecode_artifacts/named_struct_runtime_type_name/input.cd
cat /tmp/named-struct-runtime-type-name.cdbc
```

Expected: emitted text contains this named struct instruction exactly:

```text
  r1 = struct n0 {n1: r0}
```

The Rust dump step still fails until Task 3 because `vm-rs` does not parse the named struct form yet.

- [ ] **Step 9: Commit C++ IR/bytecode emission**

```sh
git add \
  include/Value.hpp include/IR.hpp src/IR.cpp include/IRCompiler.hpp src/IRCompiler.cpp \
  include/Bytecode.hpp src/Bytecode.cpp src/BytecodeCompiler.cpp src/BytecodeTextEmitter.cpp \
  docs/bytecode-text-format.md
git commit -m "feat: emit named struct type metadata"
```

---

### Task 3: Parse, format, and execute named struct metadata in Rust VM

**Files:**
- Modify: `vm-rs/src/bytecode.rs`
- Modify: `vm-rs/src/format.rs`
- Modify: `vm-rs/src/runtime.rs`
- Modify: `vm-rs/src/value.rs`
- Modify: `vm-rs/src/vm.rs`

- [ ] **Step 1: Add type name to Rust bytecode instruction**

In `vm-rs/src/bytecode.rs`, change the `Instruction::Struct` variant from:

```rust
    Struct {
        dest: usize,
        fields: Vec<(usize, usize)>,
    },
```

to:

```rust
    Struct {
        dest: usize,
        type_name: Option<usize>,
        fields: Vec<(usize, usize)>,
    },
```

- [ ] **Step 2: Parse anonymous and named struct forms**

In `vm-rs/src/format.rs`, replace the `"struct"` parse branch with:

```rust
            "struct" => {
                let (type_name, field_text) = parse_optional_struct_type_name(line, operands)?;
                Ok(Instruction::Struct {
                    dest,
                    type_name,
                    fields: parse_struct_fields(line, field_text)?,
                })
            }
```

Add this helper near `parse_struct_fields(...)`:

```rust
fn parse_optional_struct_type_name<'a>(line: usize, text: &'a str) -> Result<(Option<usize>, &'a str), ParseError> {
    let trimmed = text.trim();
    if trimmed.starts_with('{') {
        return Ok((None, trimmed));
    }
    let Some((name_text, rest)) = trimmed.split_once(' ') else {
        return Err(ParseError {
            line,
            message: "struct expects fields".to_string(),
        });
    };
    if !rest.trim_start().starts_with('{') {
        return Err(ParseError {
            line,
            message: "struct type name must be followed by fields".to_string(),
        });
    }
    Ok((Some(parse_name_ref(line, name_text)?), rest.trim_start()))
}
```

- [ ] **Step 3: Format named struct forms**

In `format_instruction(...)`, replace the `Instruction::Struct` arm with:

```rust
        Instruction::Struct {
            dest,
            type_name,
            fields,
        } => {
            let parts = fields
                .iter()
                .map(|(name, value)| format!("n{}: r{}", name, value))
                .collect::<Vec<_>>()
                .join(", ");
            match type_name {
                Some(type_name) => format!("r{} = struct n{} {{{}}}", dest, type_name, parts),
                None => format!("r{} = struct {{{}}}", dest, parts),
            }
        }
```

- [ ] **Step 4: Update Rust format tests**

In `vm-rs/src/format.rs` test `parses_all_opcode_shapes`, ensure the sample program contains both forms in the main body:

```text
  r3 = struct {n0: r1}
  r4 = struct n1 {n0: r1}
```

Update the expected round-trip string in that test to include the same two canonical lines.

Run:

```sh
cargo test --manifest-path vm-rs/Cargo.toml format::tests::parses_all_opcode_shapes
```

Expected: the focused format test passes.

- [ ] **Step 5: Add optional type name to Rust runtime struct values**

In `vm-rs/src/runtime.rs`, change `StructValue` to:

```rust
#[derive(Clone, Debug)]
pub struct StructValue {
    pub identity: usize,
    pub type_name: Option<String>,
    pub fields: SharedStructFields,
}
```

In `vm-rs/src/value.rs`, change the struct arm in `Value::type_name()` from:

```rust
            Self::Struct(_) => "struct",
```

to:

```rust
            Self::Struct(value) => value.type_name.as_deref().unwrap_or("struct"),
```

- [ ] **Step 6: Execute named struct instructions**

In `vm-rs/src/vm.rs`, change the execution match arm from:

```rust
                Instruction::Struct { dest, fields } => {
                    let value = self.make_struct(frame, fields)?;
                    self.write_register(frame, *dest, value)?;
                }
```

to:

```rust
                Instruction::Struct {
                    dest,
                    type_name,
                    fields,
                } => {
                    let type_name = type_name.map(|index| self.read_name(index)).transpose()?;
                    let value = self.make_struct(frame, type_name, fields)?;
                    self.write_register(frame, *dest, value)?;
                }
```

Change `make_struct` signature and body to:

```rust
    fn make_struct(
        &mut self,
        frame: &Frame,
        type_name: Option<String>,
        fields: &[(usize, usize)],
    ) -> Result<Value, RuntimeError> {
        let identity = self.next_struct_identity;
        self.next_struct_identity += 1;
        let mut values = Vec::with_capacity(fields.len());
        for (name_index, register) in fields {
            values.push((self.read_name(*name_index)?, self.read_register(frame, *register)?));
        }
        Ok(Value::structure(StructValue {
            identity,
            type_name,
            fields: Rc::new(RefCell::new(values)),
        }))
    }
```

- [ ] **Step 7: Run Rust VM focused checks**

```sh
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case struct_runtime_type_name \
  --case struct_runtime_type_name_namespace \
  --case native_stdlib_typeof \
  --case named_struct_runtime_type_name
```

Expected: both commands pass. `named_struct_runtime_type_name` is discovered from `tests/bytecode_artifacts/` by the Rust VM runner and uses freshly emitted bytecode. The bytecode artifact diff tests are intentionally deferred to Task 4, after expected `.cdbc` files are refreshed.

- [ ] **Step 8: Commit Rust VM support**

```sh
git add vm-rs/src/bytecode.rs vm-rs/src/format.rs vm-rs/src/runtime.rs vm-rs/src/value.rs vm-rs/src/vm.rs
git commit -m "feat: execute named struct type metadata"
```

---

### Task 4: Refresh affected goldens and artifacts

**Files:**
- Modify: `tests/golden/native_stdlib_typeof/bytecode.out`
- Modify: `tests/golden/native_stdlib_typeof/ir.out`
- Modify: `tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc`
- Verify/create: `tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc`

- [ ] **Step 1: Refresh `native_stdlib_typeof` compiler debug goldens**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --update --case native_stdlib_typeof
```

Expected: only `tests/golden/native_stdlib_typeof/ir.out`, `tests/golden/native_stdlib_typeof/bytecode.out`, and the already-edited `run.out` change for named struct metadata. Review with:

```sh
git diff -- tests/golden/native_stdlib_typeof
```

- [ ] **Step 2: Refresh bytecode artifact expected files**

```sh
./build/compiler_design --emit-bytecode \
  tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc \
  tests/bytecode_artifacts/native_stdlib_typeof/input.cd
./build/compiler_design --emit-bytecode \
  tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc \
  tests/bytecode_artifacts/named_struct_runtime_type_name/input.cd
```

Expected: both artifacts contain named struct instructions. Verify with:

```sh
grep -R "struct n" tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc \
  tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc
```

Expected output includes at least:

```text
tests/bytecode_artifacts/named_struct_runtime_type_name/expected.cdbc:  r1 = struct n0 {n1: r0}
```

- [ ] **Step 3: Run focused artifact and VM checks**

```sh
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case struct_runtime_type_name \
  --case struct_runtime_type_name_namespace \
  --case native_stdlib_typeof \
  --case named_struct_runtime_type_name
```

Expected: all selected checks pass.

- [ ] **Step 4: Commit golden and artifact refresh**

```sh
git add \
  tests/golden/native_stdlib_typeof \
  tests/bytecode_artifacts/native_stdlib_typeof/expected.cdbc \
  tests/bytecode_artifacts/named_struct_runtime_type_name
git commit -m "test: refresh named struct type metadata outputs"
```

---

### Task 5: Update user documentation and roadmap

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README named struct paragraph**

In `README.md`, replace:

```text
Named structs are static-only in this phase: runtime values remain anonymous struct values. Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, and allow fields in any order. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field annotations may refer to non-recursive struct names declared later in the same scope, but recursive struct field types such as `struct Node { next: Node? }` are explicitly rejected for now. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)` and runtime type names are not implemented yet.
```

with:

```text
Named constructor expressions such as `Person { name: "Ada", age: 36 }` infer the named static type, require an exact field match, allow fields in any order, and attach the named runtime type used by `typeOf`; anonymous struct literals still report `"struct"` and all structs keep the same field-only print format. Annotated anonymous literals such as `let p: Person = { name: "Ada", age: 36 };` remain supported. Field annotations may refer to non-recursive struct names declared later in the same scope, but recursive struct field types such as `struct Node { next: Node? }` are explicitly rejected for now. Field access/assignment on known named struct values is statically checked. Constructor functions such as `Person(...)` are not implemented yet.
```

In the `typeOf` stdlib paragraph, replace:

```text
Named struct values return `"struct"`, and arrays return `"array"` regardless of static element type.
```

with:

```text
Named struct values return their runtime struct name such as `"Person"` or `"geo.Point"`; anonymous struct values return `"struct"`; arrays return `"array"` regardless of static element type.
```

- [ ] **Step 2: Update roadmap Phase 12**

In `docs/roadmap.md`, replace:

```markdown
- Decide whether named runtime values should expose runtime type names beyond
  the current generic `struct` result.
```

with:

```markdown
- Named struct runtime values expose their static name through `typeOf`; anonymous
  struct values still report `struct`.
```

- [ ] **Step 3: Run documentation-adjacent focused tests**

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case native_stdlib_typeof
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens \
  --case struct_runtime_type_name \
  --case struct_runtime_type_name_namespace \
  --case native_stdlib_typeof
```

Expected: both commands pass.

- [ ] **Step 4: Commit docs**

```sh
git add README.md docs/roadmap.md
git commit -m "docs: document named struct runtime type names"
```

---

### Task 6: Full verification and cleanup

**Files:**
- No source changes expected.
- Remove: `tests/__pycache__/` if Python creates it.

- [ ] **Step 1: Check git status before verification**

```sh
git status --short
```

Expected: no output. If files are modified, inspect and commit intentional changes before continuing.

- [ ] **Step 2: Run full required verification**

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

Expected: every command exits 0. Record the exact summary output for the final response.

- [ ] **Step 3: Check final git status**

```sh
git status --short
```

Expected: no output after removing `tests/__pycache__/`.

- [ ] **Step 4: Prepare final implementation summary**

Report:

```text
Implemented named struct runtime type names:
- named struct constructors attach runtime type-name metadata;
- anonymous structs still report `struct`;
- `typeOf` returns `Person` / `geo.Point` for named struct values;
- struct printing, equality, fields, assignment, and methods are unchanged;
- `.cdbc` supports anonymous and named struct construction forms;
- docs and roadmap updated.

Verification run:
- cmake -S . -B build: passed
- cmake --build build: passed
- ctest --test-dir build --output-on-failure: passed
- python3 tests/run_golden_tests.py ./build/compiler_design: passed
- python3 tests/run_golden_tests_selftest.py: passed
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs: passed
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens: passed
- cargo test --manifest-path vm-rs/Cargo.toml: passed
```
