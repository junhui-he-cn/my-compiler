# Rust VM Executor Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `compiler-design-vm run <program.cdbc>` so the Rust VM executes current bytecode artifacts with stdout matching the C++ bytecode reference backend.

**Architecture:** Add focused Rust runtime modules (`value.rs`, `runtime.rs`, `vm.rs`) while keeping `bytecode.rs` and `format.rs` as the artifact boundary. Execute `.cdbc` with a direct interpreter that mirrors C++ `BytecodeVM` semantics: registers, frames, shared cells, closures, mutable arrays, and runtime errors. Add a Python integration runner that compiles `.cd` to `.cdbc`, runs Rust VM, and compares stdout.

**Tech Stack:** Rust 2021 binary crate, C++17 compiler executable, Python 3 integration tests, CMake/CTest, existing `.cdbc` text artifacts.

---

## File Structure

- Create `vm-rs/src/value.rs`: Rust runtime `Value`, `NumberText`, value formatting, truthiness, equality, and unit tests.
- Create `vm-rs/src/runtime.rs`: `Cell`, `Environment`, `FunctionValue`, `ArrayValue`, and identity/shared-object helpers.
- Create `vm-rs/src/vm.rs`: `VM`, `Frame`, `RuntimeError`, instruction dispatch, function calls, closures, arrays, and unit tests for executor internals.
- Modify `vm-rs/src/main.rs`: expose `run <program.cdbc>`, wire parse + execute, update help text and CLI tests.
- Modify `vm-rs/src/bytecode.rs`: add helper methods only if needed; avoid changing artifact shape.
- Modify `vm-rs/README.md`: update current command status for `dump` and `run`.
- Create `tests/run_rust_vm_tests.py`: compile source fixtures to `.cdbc`, run Rust VM, compare stdout/stderr/exit.
- Add `tests/bytecode_artifacts/*/run.out`: expected Rust VM stdout for artifact fixtures.
- Modify `CMakeLists.txt`: add `rust_vm` CTest entry after artifact tests.
- Modify `AGENTS.md`: add Rust VM runner verification command and workflow note.
- Modify `README.md`: document Rust VM `run` command as implemented at artifact level.
- Modify `docs/roadmap.md`: mark backend Phase 3 implemented after the executor lands.

## TDD Notes

Each implementation task starts with a failing Rust unit test or failing Python integration test. Do not write production Rust VM code before observing the relevant test fail. Keep commits small so failures are easy to bisect.

---

### Task 1: Add Rust VM integration runner and RED arithmetic fixture

**Files:**
- Create: `tests/run_rust_vm_tests.py`
- Create: `tests/bytecode_artifacts/arithmetic/run.out`
- Create: `tests/bytecode_artifacts/control_flow/run.out`
- Create: `tests/bytecode_artifacts/functions_arrays/run.out`

- [ ] **Step 1: Add expected stdout files for artifact fixtures**

Run:

```bash
cat > tests/bytecode_artifacts/arithmetic/run.out <<'EOF'
7
true
EOF
cat > tests/bytecode_artifacts/control_flow/run.out <<'EOF'
zero
one
EOF
cat > tests/bytecode_artifacts/functions_arrays/run.out <<'EOF'
2
3
EOF
```

- [ ] **Step 2: Create integration runner**

Create `tests/run_rust_vm_tests.py`:

```python
#!/usr/bin/env python3
import argparse
import difflib
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CheckResult:
    name: str
    passed: bool
    message: str = ""


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def unified_diff(expected: str, actual: str, fromfile: str, tofile: str) -> str:
    return "".join(
        difflib.unified_diff(
            expected.splitlines(keepends=True),
            actual.splitlines(keepends=True),
            fromfile=fromfile,
            tofile=tofile,
        )
    )


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True, check=False)


def discover_artifact_cases(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    return sorted(
        path for path in root.iterdir()
        if path.is_dir() and (path / "input.cd").is_file() and (path / "run.out").is_file()
    )


def discover_golden_cases(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    return sorted(
        path for path in root.iterdir()
        if path.is_dir()
        and (path / "input.cd").is_file()
        and ((path / "run_bytecode.out").is_file() or (path / "run.out").is_file())
    )


def expected_output(case_dir: Path) -> str:
    bytecode_expected = case_dir / "run_bytecode.out"
    if bytecode_expected.is_file():
        return read_text(bytecode_expected)
    return read_text(case_dir / "run.out")


def check_case(compiler: Path, vm_manifest: Path, case_dir: Path) -> list[CheckResult]:
    source = case_dir / "input.cd"
    expected = expected_output(case_dir)
    results: list[CheckResult] = []

    with tempfile.TemporaryDirectory() as temp_dir:
        artifact = Path(temp_dir) / "program.cdbc"
        compile_command = [str(compiler), "--emit-bytecode", str(artifact), str(source)]
        compiled = run_command(compile_command)
        compile_name = f"{case_dir.name} emit"
        if compiled.returncode != 0:
            results.append(CheckResult(
                compile_name,
                False,
                f"FAIL {compile_name} exited with {compiled.returncode}\n\nSTDOUT:\n{compiled.stdout}\nSTDERR:\n{compiled.stderr}",
            ))
            return results
        if compiled.stdout:
            results.append(CheckResult(compile_name, False, f"FAIL {compile_name} produced unexpected stdout\n\n{compiled.stdout}"))
        if compiled.stderr:
            results.append(CheckResult(compile_name, False, f"FAIL {compile_name} produced unexpected stderr\n\n{compiled.stderr}"))
        if not artifact.is_file():
            results.append(CheckResult(compile_name, False, f"FAIL {compile_name} did not create {artifact}"))
            return results
        if not any(result.name == compile_name and not result.passed for result in results):
            results.append(CheckResult(compile_name, True))

        run_command_line = ["cargo", "run", "--quiet", "--manifest-path", str(vm_manifest), "--", "run", str(artifact)]
        executed = run_command(run_command_line)
        run_name = f"{case_dir.name} rust-run"
        if executed.returncode != 0:
            results.append(CheckResult(
                run_name,
                False,
                f"FAIL {run_name} exited with {executed.returncode}\n\nSTDOUT:\n{executed.stdout}\nSTDERR:\n{executed.stderr}",
            ))
            return results
        if executed.stderr:
            results.append(CheckResult(run_name, False, f"FAIL {run_name} produced unexpected stderr\n\n{executed.stderr}"))
        if executed.stdout != expected:
            results.append(CheckResult(
                run_name,
                False,
                f"FAIL {run_name} stdout mismatch\n\n" + unified_diff(expected, executed.stdout, "expected", "actual"),
            ))
        else:
            results.append(CheckResult(run_name, True))

    return results


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Compiler Design Rust VM integration tests.")
    parser.add_argument("compiler", type=Path, help="Path to compiler_design executable")
    parser.add_argument("vm", type=Path, help="Path to vm-rs directory or vm-rs/Cargo.toml")
    parser.add_argument("--case", action="append", dest="cases", help="Run only a case directory name. Can be repeated.")
    parser.add_argument("--goldens", action="store_true", help="Also run selected tests/golden fixtures with run outputs.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        print(f"compiler not found: {compiler}", file=sys.stderr)
        return 64

    vm_path = args.vm.resolve()
    vm_manifest = vm_path / "Cargo.toml" if vm_path.is_dir() else vm_path
    if not vm_manifest.is_file():
        print(f"Rust VM manifest not found: {vm_manifest}", file=sys.stderr)
        return 64

    tests_root = Path(__file__).resolve().parent
    case_dirs = discover_artifact_cases(tests_root / "bytecode_artifacts")
    if args.goldens:
        golden_allowlist = {
            "array_index_assignment",
            "array_nested_assignment",
            "bytecode_arrays",
            "bytecode_control_flow",
            "bytecode_functions_closures",
            "bytecode_smoke",
            "bytecode_variables",
            "function_return_type_success",
            "function_return_type_unknown_preserved",
            "function_value_arity_success",
            "function_value_unknown_arity_assignment",
            "inferred_let_assignment",
            "inferred_let_unknown_call_result",
            "lambda_basic",
            "lambda_closure",
            "lambda_immediate_call",
            "lambda_mutable_closure",
            "len_builtin",
            "len_builtin_shadowing",
        }
        case_dirs.extend(path for path in discover_golden_cases(tests_root / "golden") if path.name in golden_allowlist)

    if args.cases:
        wanted = set(args.cases)
        case_dirs = [path for path in case_dirs if path.name in wanted]

    if not case_dirs:
        print("no Rust VM fixtures selected", file=sys.stderr)
        return 1

    results: list[CheckResult] = []
    for case_dir in case_dirs:
        results.extend(check_case(compiler, vm_manifest, case_dir))

    failed = [result for result in results if not result.passed]
    for failure in failed:
        print(failure.message, file=sys.stderr)

    passed_count = len(results) - len(failed)
    print(f"rust vm tests: {passed_count} passed, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 3: Make runner executable**

Run:

```bash
chmod +x tests/run_rust_vm_tests.py
```

- [ ] **Step 4: Run RED arithmetic integration test**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case arithmetic
```

Expected: fails because `compiler-design-vm run` exits `64` with `command \`run\` is planned but not implemented in this phase`.

- [ ] **Step 5: Commit failing integration runner**

Run:

```bash
git add tests/run_rust_vm_tests.py tests/bytecode_artifacts/*/run.out
git commit -m "test: cover rust vm execution loop"
```

---

### Task 2: Add runtime values and shared object model

**Files:**
- Create: `vm-rs/src/value.rs`
- Create: `vm-rs/src/runtime.rs`
- Modify: `vm-rs/src/main.rs`

- [ ] **Step 1: Add module declarations only**

Edit the top of `vm-rs/src/main.rs` to include:

```rust
mod bytecode;
mod format;
mod runtime;
mod value;
```

- [ ] **Step 2: Add failing value tests with stub module**

Create `vm-rs/src/value.rs`:

```rust
use std::fmt;

#[derive(Clone, Debug)]
pub enum Value {
    Nil,
}

impl Value {
    pub fn number(_value: f64) -> Self {
        Self::Nil
    }

    pub fn boolean(_value: bool) -> Self {
        Self::Nil
    }

    pub fn string(_value: impl Into<String>) -> Self {
        Self::Nil
    }

    pub fn is_truthy(&self) -> bool {
        false
    }

    pub fn runtime_equals(&self, _other: &Self) -> bool {
        false
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "nil")
    }
}

#[cfg(test)]
mod tests {
    use super::Value;

    #[test]
    fn formats_primitives_like_cpp_runtime() {
        assert_eq!(Value::Nil.to_string(), "nil");
        assert_eq!(Value::number(7.0).to_string(), "7");
        assert_eq!(Value::number(1.25).to_string(), "1.25");
        assert_eq!(Value::boolean(true).to_string(), "true");
        assert_eq!(Value::boolean(false).to_string(), "false");
        assert_eq!(Value::string("hello").to_string(), "hello");
    }

    #[test]
    fn truthiness_matches_language_runtime() {
        assert!(!Value::Nil.is_truthy());
        assert!(!Value::boolean(false).is_truthy());
        assert!(Value::boolean(true).is_truthy());
        assert!(Value::number(0.0).is_truthy());
        assert!(Value::string("").is_truthy());
    }

    #[test]
    fn primitive_equality_matches_runtime() {
        assert!(Value::Nil.runtime_equals(&Value::Nil));
        assert!(Value::number(2.0).runtime_equals(&Value::number(2.0)));
        assert!(!Value::number(2.0).runtime_equals(&Value::number(3.0)));
        assert!(Value::boolean(true).runtime_equals(&Value::boolean(true)));
        assert!(Value::string("x").runtime_equals(&Value::string("x")));
        assert!(!Value::string("x").runtime_equals(&Value::number(0.0)));
    }
}
```

- [ ] **Step 3: Add runtime shared object stubs**

Create `vm-rs/src/runtime.rs`:

```rust
use crate::value::Value;
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

pub type Cell = Rc<RefCell<Value>>;
pub type Environment = HashMap<String, Cell>;
pub type SharedEnvironment = Rc<RefCell<Environment>>;
pub type SharedArrayElements = Rc<RefCell<Vec<Value>>>;

#[derive(Clone, Debug)]
pub struct FunctionValue {
    pub name: String,
    pub function_index: usize,
    pub arity: usize,
    pub identity: usize,
    pub closure: SharedEnvironment,
}

#[derive(Clone, Debug)]
pub struct ArrayValue {
    pub identity: usize,
    pub elements: SharedArrayElements,
}

pub fn new_environment() -> SharedEnvironment {
    Rc::new(RefCell::new(HashMap::new()))
}

pub fn new_cell(value: Value) -> Cell {
    Rc::new(RefCell::new(value))
}
```

- [ ] **Step 4: Run RED value tests**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml value::tests
```

Expected: `formats_primitives_like_cpp_runtime`, `truthiness_matches_language_runtime`, and `primitive_equality_matches_runtime` fail because `number`, `boolean`, `string`, and equality are stubbed.

- [ ] **Step 5: Implement runtime `Value`**

Replace `vm-rs/src/value.rs` with:

```rust
use crate::runtime::{ArrayValue, FunctionValue};
use std::fmt;

#[derive(Clone, Debug)]
pub enum Value {
    Nil,
    Number(f64),
    Bool(bool),
    String(String),
    Function(FunctionValue),
    Array(ArrayValue),
}

impl Value {
    pub fn number(value: f64) -> Self {
        Self::Number(value)
    }

    pub fn boolean(value: bool) -> Self {
        Self::Bool(value)
    }

    pub fn string(value: impl Into<String>) -> Self {
        Self::String(value.into())
    }

    pub fn function(value: FunctionValue) -> Self {
        Self::Function(value)
    }

    pub fn array(value: ArrayValue) -> Self {
        Self::Array(value)
    }

    pub fn type_name(&self) -> &'static str {
        match self {
            Self::Nil => "nil",
            Self::Number(_) => "number",
            Self::Bool(_) => "bool",
            Self::String(_) => "string",
            Self::Function(_) => "function",
            Self::Array(_) => "array",
        }
    }

    pub fn is_truthy(&self) -> bool {
        !matches!(self, Self::Nil | Self::Bool(false))
    }

    pub fn runtime_equals(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Nil, Self::Nil) => true,
            (Self::Number(left), Self::Number(right)) => left == right,
            (Self::Bool(left), Self::Bool(right)) => left == right,
            (Self::String(left), Self::String(right)) => left == right,
            (Self::Function(left), Self::Function(right)) => left.identity == right.identity,
            (Self::Array(left), Self::Array(right)) => left.identity == right.identity,
            _ => false,
        }
    }
}

fn format_number(value: f64) -> String {
    if value.fract() == 0.0 {
        format!("{:.0}", value)
    } else {
        value.to_string()
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Nil => write!(f, "nil"),
            Self::Number(value) => write!(f, "{}", format_number(*value)),
            Self::Bool(value) => write!(f, "{}", if *value { "true" } else { "false" }),
            Self::String(value) => write!(f, "{}", value),
            Self::Function(function) => write!(f, "<fn {}>", function.name),
            Self::Array(array) => {
                write!(f, "[")?;
                let elements = array.elements.borrow();
                for (index, value) in elements.iter().enumerate() {
                    if index != 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", value)?;
                }
                write!(f, "]")
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Value;

    #[test]
    fn formats_primitives_like_cpp_runtime() {
        assert_eq!(Value::Nil.to_string(), "nil");
        assert_eq!(Value::number(7.0).to_string(), "7");
        assert_eq!(Value::number(1.25).to_string(), "1.25");
        assert_eq!(Value::boolean(true).to_string(), "true");
        assert_eq!(Value::boolean(false).to_string(), "false");
        assert_eq!(Value::string("hello").to_string(), "hello");
    }

    #[test]
    fn truthiness_matches_language_runtime() {
        assert!(!Value::Nil.is_truthy());
        assert!(!Value::boolean(false).is_truthy());
        assert!(Value::boolean(true).is_truthy());
        assert!(Value::number(0.0).is_truthy());
        assert!(Value::string("").is_truthy());
    }

    #[test]
    fn primitive_equality_matches_runtime() {
        assert!(Value::Nil.runtime_equals(&Value::Nil));
        assert!(Value::number(2.0).runtime_equals(&Value::number(2.0)));
        assert!(!Value::number(2.0).runtime_equals(&Value::number(3.0)));
        assert!(Value::boolean(true).runtime_equals(&Value::boolean(true)));
        assert!(Value::string("x").runtime_equals(&Value::string("x")));
        assert!(!Value::string("x").runtime_equals(&Value::number(0.0)));
    }
}
```

- [ ] **Step 6: Run GREEN value tests**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml value::tests
```

Expected: all value tests pass.

- [ ] **Step 7: Commit value/runtime modules**

Run:

```bash
git add vm-rs/src/main.rs vm-rs/src/value.rs vm-rs/src/runtime.rs
git commit -m "feat: add rust vm runtime values"
```

---

### Task 3: Implement `run` CLI and basic VM for constants, print, arithmetic, and comparisons

**Files:**
- Create: `vm-rs/src/vm.rs`
- Modify: `vm-rs/src/main.rs`
- Test: `tests/run_rust_vm_tests.py --case arithmetic`

- [ ] **Step 1: Add VM module declaration**

At the top of `vm-rs/src/main.rs`, include:

```rust
mod vm;
```

- [ ] **Step 2: Add failing CLI test for implemented `run` help**

In `vm-rs/src/main.rs`, replace the test module with:

```rust
#[cfg(test)]
mod tests {
    use super::help_text;

    #[test]
    fn help_mentions_dump_and_run_scope() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
        assert!(help.contains("compiler-design-vm run <program.cdbc>"));
        assert!(help.contains(".cdbc parsing, canonical dump, and bytecode execution are implemented"));
    }
}
```

- [ ] **Step 3: Run RED CLI test**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml tests::help_mentions_dump_and_run_scope
```

Expected: fails because help still says bytecode execution is not implemented.

- [ ] **Step 4: Add basic VM implementation**

Create `vm-rs/src/vm.rs` with this initial executor:

```rust
use crate::bytecode::{Constant, FunctionBody, Instruction, Program};
use crate::value::Value;
use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RuntimeError {
    pub message: String,
}

impl RuntimeError {
    fn new(message: impl Into<String>) -> Self {
        Self { message: message.into() }
    }
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "runtime error: {}", self.message)
    }
}

struct Frame {
    ip: usize,
    registers: Vec<Value>,
}

pub struct VM<'a> {
    program: &'a Program,
    output: String,
}

impl<'a> VM<'a> {
    pub fn new(program: &'a Program) -> Self {
        Self { program, output: String::new() }
    }

    pub fn run(mut self) -> Result<String, RuntimeError> {
        let mut frame = Frame { ip: 0, registers: vec![Value::Nil; self.program.main.registers] };
        self.execute_body(&self.program.main, &mut frame)?;
        Ok(self.output)
    }

    fn execute_body(&mut self, body: &FunctionBody, frame: &mut Frame) -> Result<Option<Value>, RuntimeError> {
        frame.ip = 0;
        while frame.ip < body.instructions.len() {
            let instruction = &body.instructions[frame.ip];
            match instruction {
                Instruction::Constant { dest, constant } => {
                    let value = self.constant_value(*constant)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::Print { value } => {
                    let value = self.read_register(frame, *value)?;
                    self.output.push_str(&value.to_string());
                    self.output.push('\n');
                }
                Instruction::Negate { dest, value } => {
                    let input = self.expect_number(frame, *value, "negate")?;
                    self.write_register(frame, *dest, Value::number(-input))?;
                }
                Instruction::Not { dest, value } => {
                    let result = !self.read_register(frame, *value)?.is_truthy();
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::Add { dest, left, right } => {
                    let left_value = self.read_register(frame, *left)?;
                    let right_value = self.read_register(frame, *right)?;
                    let result = match (left_value, right_value) {
                        (Value::Number(left), Value::Number(right)) => Value::number(left + right),
                        (Value::String(left), Value::String(right)) => Value::string(format!("{}{}", left, right)),
                        _ => return Err(RuntimeError::new("add expects two numbers or two strings")),
                    };
                    self.write_register(frame, *dest, result)?;
                }
                Instruction::Subtract { dest, left, right } => {
                    let (left, right) = self.expect_two_numbers(frame, *left, *right, "subtract")?;
                    self.write_register(frame, *dest, Value::number(left - right))?;
                }
                Instruction::Multiply { dest, left, right } => {
                    let (left, right) = self.expect_two_numbers(frame, *left, *right, "multiply")?;
                    self.write_register(frame, *dest, Value::number(left * right))?;
                }
                Instruction::Divide { dest, left, right } => {
                    let (left, right) = self.expect_two_numbers(frame, *left, *right, "divide")?;
                    if right == 0.0 {
                        return Err(RuntimeError::new("division by zero"));
                    }
                    self.write_register(frame, *dest, Value::number(left / right))?;
                }
                Instruction::Equal { dest, left, right } => {
                    let result = self.read_register(frame, *left)?.runtime_equals(&self.read_register(frame, *right)?);
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::NotEqual { dest, left, right } => {
                    let result = !self.read_register(frame, *left)?.runtime_equals(&self.read_register(frame, *right)?);
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::Greater { dest, left, right } => self.compare(frame, *dest, *left, *right, "greater", |l, r| l > r)?,
                Instruction::GreaterEqual { dest, left, right } => self.compare(frame, *dest, *left, *right, "greater_equal", |l, r| l >= r)?,
                Instruction::Less { dest, left, right } => self.compare(frame, *dest, *left, *right, "less", |l, r| l < r)?,
                Instruction::LessEqual { dest, left, right } => self.compare(frame, *dest, *left, *right, "less_equal", |l, r| l <= r)?,
                Instruction::Return { value } => return Ok(Some(self.read_register(frame, *value)?)),
                other => return Err(RuntimeError::new(format!("unsupported instruction in current VM slice: {:?}", other))),
            }
            frame.ip += 1;
        }
        Ok(None)
    }

    fn constant_value(&self, index: usize) -> Result<Value, RuntimeError> {
        let constant = self.program.constants.get(index).ok_or_else(|| RuntimeError::new("constant index out of range"))?;
        match constant {
            Constant::Nil => Ok(Value::Nil),
            Constant::Number(value) => value.parse::<f64>().map(Value::number).map_err(|_| RuntimeError::new("invalid number constant")),
            Constant::Bool(value) => Ok(Value::boolean(*value)),
            Constant::String(value) => Ok(Value::string(value.clone())),
        }
    }

    fn read_register(&self, frame: &Frame, index: usize) -> Result<Value, RuntimeError> {
        frame.registers.get(index).cloned().ok_or_else(|| RuntimeError::new("register index out of range"))
    }

    fn write_register(&self, frame: &mut Frame, index: usize, value: Value) -> Result<(), RuntimeError> {
        let slot = frame.registers.get_mut(index).ok_or_else(|| RuntimeError::new("register index out of range"))?;
        *slot = value;
        Ok(())
    }

    fn expect_number(&self, frame: &Frame, value: usize, op_name: &str) -> Result<f64, RuntimeError> {
        match self.read_register(frame, value)? {
            Value::Number(value) => Ok(value),
            other => Err(RuntimeError::new(format!("{} expects number, got {}", op_name, other.type_name()))),
        }
    }

    fn expect_two_numbers(&self, frame: &Frame, left: usize, right: usize, op_name: &str) -> Result<(f64, f64), RuntimeError> {
        match (self.read_register(frame, left)?, self.read_register(frame, right)?) {
            (Value::Number(left), Value::Number(right)) => Ok((left, right)),
            _ => Err(RuntimeError::new(format!("{} expects numbers", op_name))),
        }
    }

    fn compare(&self, frame: &mut Frame, dest: usize, left: usize, right: usize, op_name: &str, operation: fn(f64, f64) -> bool) -> Result<(), RuntimeError> {
        let (left, right) = self.expect_two_numbers(frame, left, right, op_name)?;
        self.write_register(frame, dest, Value::boolean(operation(left, right)))
    }
}
```

- [ ] **Step 5: Wire `run` CLI**

Update `vm-rs/src/main.rs` as follows:

1. Add `mod vm;` at the top if not already present.
2. Change help text to:

```rust
const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm dump <program.cdbc>\n\
  compiler-design-vm run <program.cdbc>\n\n\
Current phase: .cdbc parsing, canonical dump, and bytecode execution are implemented.\n";
```

3. Add this function below `dump`:

```rust
fn run(path: &str) -> Result<(), String> {
    let source = fs::read_to_string(path).map_err(|error| format!("error: failed to read `{}`: {}", path, error))?;
    let program = format::parse_program(&source).map_err(|error| format!("error: {}", error))?;
    let output = vm::VM::new(&program).run().map_err(|error| format!("error: {}", error))?;
    print!("{}", output);
    Ok(())
}
```

4. Replace the existing `Some("run")` arm with:

```rust
Some("run") => {
    let Some(path) = args.next() else {
        eprintln!("error: run expects <program.cdbc>");
        eprintln!();
        eprint!("{}", help_text());
        process::exit(64);
    };
    if args.next().is_some() {
        eprintln!("error: run expects exactly one input file");
        eprintln!();
        eprint!("{}", help_text());
        process::exit(64);
    }
    if let Err(error) = run(&path) {
        eprintln!("{}", error);
        process::exit(1);
    }
}
```

- [ ] **Step 6: Run GREEN arithmetic checks**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml tests::help_mentions_dump_and_run_scope
cargo test --manifest-path vm-rs/Cargo.toml value::tests
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case arithmetic
```

Expected: CLI/help tests pass, value tests pass, arithmetic integration reports `2 passed, 0 failed`.

- [ ] **Step 7: Commit basic VM slice**

Run:

```bash
git add vm-rs/src/main.rs vm-rs/src/vm.rs
git commit -m "feat: execute basic bytecode in rust vm"
```

---

### Task 4: Add variables, moves, and jumps for control flow

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Test: `tests/run_rust_vm_tests.py --case control_flow`

- [ ] **Step 1: Run RED control-flow integration test**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case control_flow
```

Expected: fails with `unsupported instruction in current VM slice` for `StoreVar`, `LoadVar`, `JumpIfFalse`, `AssignVar`, `Jump`, or `Move`.

- [ ] **Step 2: Extend `Frame` and `VM` state**

In `vm-rs/src/vm.rs`, add imports:

```rust
use crate::runtime::{new_cell, new_environment, Cell, Environment, SharedEnvironment};
use std::collections::HashMap;
```

Replace `Frame` with:

```rust
struct Frame {
    ip: usize,
    registers: Vec<Value>,
    locals: SharedEnvironment,
    closure: SharedEnvironment,
    is_main: bool,
}
```

Replace `VM` with:

```rust
pub struct VM<'a> {
    program: &'a Program,
    globals: SharedEnvironment,
    output: String,
    next_function_identity: usize,
    next_array_identity: usize,
}
```

Update `VM::new`:

```rust
pub fn new(program: &'a Program) -> Self {
    Self {
        program,
        globals: new_environment(),
        output: String::new(),
        next_function_identity: 1,
        next_array_identity: 1,
    }
}
```

Update main frame creation in `run`:

```rust
let mut frame = Frame {
    ip: 0,
    registers: vec![Value::Nil; self.program.main.registers],
    locals: new_environment(),
    closure: new_environment(),
    is_main: true,
};
```

- [ ] **Step 3: Add environment helpers**

Add these methods inside `impl VM<'a>`:

```rust
fn read_name(&self, index: usize) -> Result<String, RuntimeError> {
    self.program.names.get(index).cloned().ok_or_else(|| RuntimeError::new("name index out of range"))
}

fn find_cell(&self, frame: &Frame, name: &str) -> Option<Cell> {
    if let Some(cell) = frame.locals.borrow().get(name) {
        return Some(cell.clone());
    }
    if let Some(cell) = frame.closure.borrow().get(name) {
        return Some(cell.clone());
    }
    self.globals.borrow().get(name).cloned()
}

fn load_variable(&self, frame: &Frame, name: &str) -> Result<Value, RuntimeError> {
    let cell = self.find_cell(frame, name).ok_or_else(|| RuntimeError::new(format!("undefined variable `{}`", name)))?;
    Ok(cell.borrow().clone())
}

fn store_variable(&self, frame: &mut Frame, name: String, value: Value) {
    let cell = new_cell(value);
    if frame.is_main {
        self.globals.borrow_mut().insert(name, cell);
    } else {
        frame.locals.borrow_mut().insert(name, cell);
    }
}

fn assign_variable(&self, frame: &Frame, name: &str, value: Value) -> Result<(), RuntimeError> {
    let cell = self.find_cell(frame, name).ok_or_else(|| RuntimeError::new(format!("undefined variable `{}`", name)))?;
    *cell.borrow_mut() = value;
    Ok(())
}

fn validate_jump_target(&self, target: usize, instruction_count: usize) -> Result<(), RuntimeError> {
    if target > instruction_count {
        Err(RuntimeError::new("jump target out of range"))
    } else {
        Ok(())
    }
}
```

Remove unused `Environment` or `HashMap` imports if Rust reports them unused after the full task.

- [ ] **Step 4: Add instruction arms**

In `execute_body`, add arms:

```rust
Instruction::Move { dest, source } => {
    let value = self.read_register(frame, *source)?;
    self.write_register(frame, *dest, value)?;
}
Instruction::LoadVar { dest, name } => {
    let name = self.read_name(*name)?;
    let value = self.load_variable(frame, &name)?;
    self.write_register(frame, *dest, value)?;
}
Instruction::StoreVar { name, value } => {
    let name = self.read_name(*name)?;
    let value = self.read_register(frame, *value)?;
    self.store_variable(frame, name, value);
}
Instruction::AssignVar { name, value } => {
    let name = self.read_name(*name)?;
    let value = self.read_register(frame, *value)?;
    self.assign_variable(frame, &name, value)?;
}
Instruction::Jump { target } => {
    self.validate_jump_target(*target, body.instructions.len())?;
    frame.ip = *target;
    continue;
}
Instruction::JumpIfFalse { condition, target } => {
    self.validate_jump_target(*target, body.instructions.len())?;
    if !self.read_register(frame, *condition)?.is_truthy() {
        frame.ip = *target;
        continue;
    }
}
Instruction::JumpIfTrue { condition, target } => {
    self.validate_jump_target(*target, body.instructions.len())?;
    if self.read_register(frame, *condition)?.is_truthy() {
        frame.ip = *target;
        continue;
    }
}
```

- [ ] **Step 5: Run GREEN control-flow checks**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case control_flow
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case arithmetic
```

Expected: Rust tests pass; arithmetic and control-flow integration tests pass.

- [ ] **Step 6: Commit control-flow slice**

Run:

```bash
git add vm-rs/src/vm.rs
git commit -m "feat: execute rust vm variables and jumps"
```

---

### Task 5: Add arrays, index assignment, and len

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Modify: `vm-rs/src/value.rs` if array formatting needs adjustment
- Test: selected golden fixtures and `functions_arrays` will still fail on functions until Task 6, but array-only goldens should pass

- [ ] **Step 1: Run RED array fixture**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case array_index_assignment
```

Expected: fails with unsupported `Array`, `Index`, `AssignIndex`, or `Len` depending on fixture bytecode.

- [ ] **Step 2: Import array runtime helpers**

In `vm-rs/src/vm.rs`, update runtime imports:

```rust
use crate::runtime::{new_cell, new_environment, ArrayValue, Cell, SharedEnvironment};
use std::cell::RefCell;
use std::rc::Rc;
```

- [ ] **Step 3: Add array/index/len helpers**

Add methods inside `impl VM<'a>`:

```rust
fn make_array(&mut self, elements: Vec<Value>) -> Value {
    let identity = self.next_array_identity;
    self.next_array_identity += 1;
    Value::array(ArrayValue { identity, elements: Rc::new(RefCell::new(elements)) })
}

fn checked_array_index(&self, index_value: Value) -> Result<usize, RuntimeError> {
    let Value::Number(number) = index_value else {
        return Err(RuntimeError::new("array index must be number"));
    };
    let integer = number.trunc();
    if integer != number {
        return Err(RuntimeError::new("array index must be integer"));
    }
    if integer < 0.0 {
        return Err(RuntimeError::new("array index out of range"));
    }
    Ok(integer as usize)
}

fn execute_index(&self, collection: Value, index: Value) -> Result<Value, RuntimeError> {
    let Value::Array(array) = collection else {
        return Err(RuntimeError::new("can only index arrays"));
    };
    let position = self.checked_array_index(index)?;
    let elements = array.elements.borrow();
    elements.get(position).cloned().ok_or_else(|| RuntimeError::new("array index out of range"))
}

fn execute_assign_index(&self, collection: Value, index: Value, value: Value) -> Result<Value, RuntimeError> {
    let Value::Array(array) = collection else {
        return Err(RuntimeError::new("can only assign array elements"));
    };
    let position = self.checked_array_index(index)?;
    let mut elements = array.elements.borrow_mut();
    if position >= elements.len() {
        return Err(RuntimeError::new("array index out of range"));
    }
    elements[position] = value.clone();
    Ok(value)
}

fn execute_len(&self, value: Value) -> Result<Value, RuntimeError> {
    match value {
        Value::Array(array) => Ok(Value::number(array.elements.borrow().len() as f64)),
        Value::String(value) => Ok(Value::number(value.len() as f64)),
        _ => Err(RuntimeError::new("len expects array or string")),
    }
}
```

- [ ] **Step 4: Add array instruction arms**

In `execute_body`, add arms:

```rust
Instruction::Array { dest, elements } => {
    let mut values = Vec::with_capacity(elements.len());
    for element in elements {
        values.push(self.read_register(frame, *element)?);
    }
    let value = self.make_array(values);
    self.write_register(frame, *dest, value)?;
}
Instruction::Index { dest, collection, index } => {
    let collection = self.read_register(frame, *collection)?;
    let index = self.read_register(frame, *index)?;
    let value = self.execute_index(collection, index)?;
    self.write_register(frame, *dest, value)?;
}
Instruction::AssignIndex { dest, collection, index, value } => {
    let collection = self.read_register(frame, *collection)?;
    let index = self.read_register(frame, *index)?;
    let value = self.read_register(frame, *value)?;
    let assigned = self.execute_assign_index(collection, index, value)?;
    self.write_register(frame, *dest, assigned)?;
}
Instruction::Len { dest, value } => {
    let value = self.read_register(frame, *value)?;
    let length = self.execute_len(value)?;
    self.write_register(frame, *dest, length)?;
}
```

- [ ] **Step 5: Run GREEN array checks**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case array_index_assignment
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case array_nested_assignment
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case len_builtin
```

Expected: selected array and len cases pass.

- [ ] **Step 6: Commit array/len slice**

Run:

```bash
git add vm-rs/src/vm.rs vm-rs/src/value.rs
git commit -m "feat: execute rust vm arrays and len"
```

---

### Task 6: Add functions, calls, closures, and shared cells

**Files:**
- Modify: `vm-rs/src/vm.rs`
- Modify: `vm-rs/src/runtime.rs` if helper constructors are useful
- Test: `functions_arrays`, closure/lambda/function goldens

- [ ] **Step 1: Run RED function/closure fixture**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case functions_arrays
```

Expected: fails with unsupported `MakeFunction` or `Call`.

- [ ] **Step 2: Import `FunctionValue`**

In `vm-rs/src/vm.rs`, update runtime imports:

```rust
use crate::runtime::{new_cell, new_environment, ArrayValue, Cell, FunctionValue, SharedEnvironment};
```

- [ ] **Step 3: Add closure capture helper**

Add this method inside `impl VM<'a>`:

```rust
fn capture_environment(&self, frame: &Frame) -> SharedEnvironment {
    let captured = new_environment();
    {
        let mut target = captured.borrow_mut();
        for (name, cell) in frame.closure.borrow().iter() {
            target.insert(name.clone(), cell.clone());
        }
        for (name, cell) in frame.locals.borrow().iter() {
            target.insert(name.clone(), cell.clone());
        }
    }
    captured
}
```

- [ ] **Step 4: Add function creation helper**

Add this method:

```rust
fn make_function(&mut self, function_index: usize, frame: &Frame) -> Result<Value, RuntimeError> {
    let function = self.program.functions.get(function_index).ok_or_else(|| RuntimeError::new("function index out of range"))?;
    let identity = self.next_function_identity;
    self.next_function_identity += 1;
    Ok(Value::function(FunctionValue {
        name: function.name.clone(),
        function_index,
        arity: function.params.len(),
        identity,
        closure: self.capture_environment(frame),
    }))
}
```

- [ ] **Step 5: Add function call helper**

Add this method:

```rust
fn call_function(&mut self, function: FunctionValue, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
    let bytecode_function = self.program.functions.get(function.function_index).ok_or_else(|| RuntimeError::new("function index out of range"))?;
    if arguments.len() != bytecode_function.params.len() {
        return Err(RuntimeError::new(format!("expected {} arguments but got {}", bytecode_function.params.len(), arguments.len())));
    }

    let mut frame = Frame {
        ip: 0,
        registers: vec![Value::Nil; bytecode_function.registers],
        locals: new_environment(),
        closure: function.closure.clone(),
        is_main: false,
    };

    for (index, argument) in arguments.into_iter().enumerate() {
        frame.locals.borrow_mut().insert(bytecode_function.params[index].clone(), new_cell(argument));
    }

    let body = FunctionBody { registers: bytecode_function.registers, instructions: bytecode_function.instructions.clone() };
    let result = self.execute_body(&body, &mut frame)?;
    Ok(result.unwrap_or(Value::Nil))
}
```

If Rust borrow checker rejects borrowing `self.program.functions` across `self.execute_body`, clone the needed function metadata before creating the frame:

```rust
let params = bytecode_function.params.clone();
let registers = bytecode_function.registers;
let instructions = bytecode_function.instructions.clone();
```

Then build `FunctionBody { registers, instructions }`.

- [ ] **Step 6: Add `MakeFunction` and `Call` instruction arms**

In `execute_body`, add arms:

```rust
Instruction::MakeFunction { dest, function } => {
    let value = self.make_function(*function, frame)?;
    self.write_register(frame, *dest, value)?;
}
Instruction::Call { dest, callee, arguments } => {
    let callee = self.read_register(frame, *callee)?;
    let Value::Function(function) = callee else {
        return Err(RuntimeError::new("can only call functions"));
    };
    let mut values = Vec::with_capacity(arguments.len());
    for argument in arguments {
        values.push(self.read_register(frame, *argument)?);
    }
    let result = self.call_function(function, values)?;
    self.write_register(frame, *dest, result)?;
}
```

- [ ] **Step 7: Run GREEN function/closure checks**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --case functions_arrays
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case bytecode_functions_closures
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case lambda_closure
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case lambda_mutable_closure
```

Expected: selected function, closure, and lambda cases pass.

- [ ] **Step 8: Run full Rust VM selected goldens**

Run:

```bash
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: all selected artifact and golden cases pass.

- [ ] **Step 9: Commit function/closure slice**

Run:

```bash
git add vm-rs/src/vm.rs vm-rs/src/runtime.rs
git commit -m "feat: execute rust vm functions and closures"
```

---

### Task 7: Add CTest integration and update docs

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `vm-rs/README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Add Rust VM runner to CTest**

In `CMakeLists.txt`, add after the `bytecode_artifacts` test:

```cmake
add_test(
    NAME rust_vm
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_rust_vm_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs --goldens
)
```

- [ ] **Step 2: Update top-level README**

In `README.md`, update the backend artifact paragraph so it says:

```markdown
The Rust VM can parse, dump, and execute `.cdbc` artifacts:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

The C++ `--run-bytecode` mode remains the reference backend while Rust VM execution is developed further.
```

Use four backticks for the outer edit if needed to avoid breaking Markdown fences.

- [ ] **Step 3: Update Rust VM README**

Replace `vm-rs/README.md` with:

```markdown
# Compiler Design VM

`compiler-design-vm` is the standalone Rust bytecode VM for Compiler Design `.cdbc` artifacts.

## Current Commands

```sh
cargo run --manifest-path vm-rs/Cargo.toml -- --help
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- run program.cdbc
```

`dump` parses and prints canonical `.cdbc` text. `run` executes the artifact and writes program output to stdout.

## Module Boundaries

- `bytecode`: parsed bytecode structures.
- `format`: `.cdbc` parser and serializer.
- `value`: runtime values, printing, truthiness, and equality.
- `runtime`: shared cells, environments, functions, and arrays.
- `vm`: executor, frames, instruction dispatch, calls, and runtime errors.

Future backend tracks may add GC-aware heap ownership, task scheduling, and JIT metadata modules.
```

- [ ] **Step 4: Update AGENTS verification commands**

In `AGENTS.md`, add this command to the verification block after artifact tests:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Add this workflow note near bytecode artifact formatting:

```markdown
When changing Rust VM execution semantics, update `vm-rs/src/vm.rs`, focused Rust unit tests, and `tests/run_rust_vm_tests.py` coverage together. Keep C++ `--run-bytecode` as the reference behavior until a later migration explicitly changes that policy.
```

- [ ] **Step 5: Update roadmap Phase 3**

In `docs/roadmap.md`, change:

```markdown
- Phase 3: add Rust VM executor parity for current bytecode semantics.
```

to:

```markdown
- Phase 3: add Rust VM executor parity for current bytecode semantics. Implemented.
```

- [ ] **Step 6: Run docs/CTest verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

Expected: all checks pass.

- [ ] **Step 7: Commit docs and CTest integration**

Run:

```bash
git add CMakeLists.txt README.md vm-rs/README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document rust vm execution"
```

---

### Task 8: Final verification and branch completion

**Files:**
- Verify all implementation files, tests, and docs.

- [ ] **Step 1: Run full clean verification**

Run:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
rm -rf tests/__pycache__
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected:

- CMake configure/build exits `0`.
- CTest passes, including `rust_vm`.
- Existing golden tests pass.
- Golden selftests pass.
- Artifact dump tests pass.
- Rust VM execution tests pass.
- Rust unit tests pass.

- [ ] **Step 2: Verify CLI manually**

Run:

```bash
./build/compiler_design --emit-bytecode /tmp/compiler-design-functions.cdbc tests/bytecode_artifacts/functions_arrays/input.cd
cargo run --manifest-path vm-rs/Cargo.toml -- run /tmp/compiler-design-functions.cdbc
cargo run --manifest-path vm-rs/Cargo.toml -- dump /tmp/compiler-design-functions.cdbc > /tmp/compiler-design-functions-dump.cdbc
diff -u /tmp/compiler-design-functions.cdbc /tmp/compiler-design-functions-dump.cdbc
```

Expected `run` stdout:

```text
2
3
```

Expected `diff` exits `0`.

- [ ] **Step 3: Review status and diff**

Run:

```bash
git diff --stat HEAD~8..HEAD
git diff --name-status HEAD~8..HEAD
git status --short --branch
```

Expected: diff includes Rust VM executor modules, Rust VM tests, docs, and CTest integration. Working tree is clean.

- [ ] **Step 4: Complete branch**

Use `superpowers:verification-before-completion` to report exact verification results, then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options.
