# Bytecode Artifact Emitter and Rust VM Dump Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `.cdbc` bytecode artifact loop where `compiler_design --emit-bytecode output.cdbc input.cd` writes canonical bytecode text and `compiler-design-vm dump output.cdbc` parses and reprints that canonical text.

**Architecture:** Add a C++ `BytecodeTextEmitter` that formats existing `BytecodeProgram` data without changing debug `--bytecode` output or C++ VM execution. Add Rust artifact data structures plus a hand-written line parser/formatter behind the `dump` command. Add integration tests that compile `.cd` fixtures to `.cdbc`, run Rust dump, and compare both outputs to the same expected artifact.

**Tech Stack:** C++17, CMake/CTest, Python 3 integration test runner, Rust 2021 Cargo binary crate, existing Compiler Design golden tests.

---

## File Structure

- `include/BytecodeTextEmitter.hpp`: declare `writeBytecodeText(std::ostream&, const BytecodeProgram&)`.
- `src/BytecodeTextEmitter.cpp`: canonical `.cdbc` emitter for all current `BytecodeOp` values.
- `src/main.cpp`: parse `--emit-bytecode output.cdbc`, compile source to bytecode, render artifact to memory, then write output file.
- `CMakeLists.txt`: compile `src/BytecodeTextEmitter.cpp`; add CTest artifact test after the runner is stable.
- `vm-rs/src/bytecode.rs`: Rust structures for `.cdbc` artifact data.
- `vm-rs/src/format.rs`: hand-written parser and canonical formatter with unit tests.
- `vm-rs/src/main.rs`: replace Phase 0 placeholder with `dump <file.cdbc>` command dispatch.
- `tests/run_bytecode_artifact_tests.py`: integration runner for compiler emitter and Rust dump.
- `tests/bytecode_artifacts/arithmetic/`: fixture covering constants, arithmetic, comparison, print.
- `tests/bytecode_artifacts/control_flow/`: fixture covering variables, if/else, while, jumps.
- `tests/bytecode_artifacts/functions_arrays/`: fixture covering functions, closures, calls, arrays, len, index assignment.
- `docs/bytecode-text-format.md`: update from direction document to finalized line-oriented phase format.
- `README.md`: document `--emit-bytecode` and Rust `dump` as implemented format-layer commands.
- `AGENTS.md`: add artifact workflow and verification command.
- `docs/roadmap.md`: mark backend Phase 1 and Phase 2 format work implemented after code lands.

---

### Task 1: Add failing artifact integration tests and fixtures

**Files:**
- Create: `tests/run_bytecode_artifact_tests.py`
- Create: `tests/bytecode_artifacts/arithmetic/input.cd`
- Create: `tests/bytecode_artifacts/arithmetic/expected.cdbc`
- Create: `tests/bytecode_artifacts/control_flow/input.cd`
- Create: `tests/bytecode_artifacts/control_flow/expected.cdbc`
- Create: `tests/bytecode_artifacts/functions_arrays/input.cd`
- Create: `tests/bytecode_artifacts/functions_arrays/expected.cdbc`

- [ ] **Step 1: Create artifact fixture directories**

Run:

```bash
mkdir -p tests/bytecode_artifacts/arithmetic \
  tests/bytecode_artifacts/control_flow \
  tests/bytecode_artifacts/functions_arrays
```

- [ ] **Step 2: Create arithmetic fixture**

Run:

```bash
cat > tests/bytecode_artifacts/arithmetic/input.cd <<'CD'
print 1 + 2 * 3;
print 4 == 4;
CD
cat > tests/bytecode_artifacts/arithmetic/expected.cdbc <<'CDBC'
cdbc 0.1

constants:
  c0 = number 1
  c1 = number 2
  c2 = number 3
  c3 = number 4
  c4 = number 4

names:

main registers=8:
  r0 = constant c0
  r1 = constant c1
  r2 = constant c2
  r3 = multiply r1, r2
  r4 = add r0, r3
  print r4
  r5 = constant c3
  r6 = constant c4
  r7 = equal r5, r6
  print r7
CDBC
```

- [ ] **Step 3: Create control-flow fixture**

Run:

```bash
cat > tests/bytecode_artifacts/control_flow/input.cd <<'CD'
let i = 0;
while i < 2 {
  if i == 1 {
    print "one";
  } else {
    print "zero";
  }
  i = i + 1;
}
CD
cat > tests/bytecode_artifacts/control_flow/expected.cdbc <<'CDBC'
cdbc 0.1

constants:
  c0 = number 0
  c1 = number 2
  c2 = number 1
  c3 = string "one"
  c4 = string "zero"
  c5 = number 1

names:
  n0 = "i#0"
  n1 = "i#0"
  n2 = "i#0"
  n3 = "i#0"
  n4 = "i#0"

main registers=12:
  r0 = constant c0
  store_var n0, r0
  r1 = load_var n1
  r2 = constant c1
  r3 = less r1, r2
  jump_if_false r3, 20
  r4 = load_var n2
  r5 = constant c2
  r6 = equal r4, r5
  jump_if_false r6, 13
  r7 = constant c3
  print r7
  jump 15
  r8 = constant c4
  print r8
  r9 = load_var n3
  r10 = constant c5
  r11 = add r9, r10
  assign_var n4, r11
  jump 2
CDBC
```

- [ ] **Step 4: Create functions/arrays fixture**

Run:

```bash
cat > tests/bytecode_artifacts/functions_arrays/input.cd <<'CD'
fun makeCounter(start) {
  let value = start;
  fun next() {
    value = value + 1;
    return value;
  }
  return next;
}
let counter = makeCounter(1);
print counter();
let xs = [1, 2, 3];
xs[1] = len(xs);
print xs[1];
CD
cat > tests/bytecode_artifacts/functions_arrays/expected.cdbc <<'CDBC'
cdbc 0.1

constants:
  c0 = nil
  c1 = nil
  c2 = number 1
  c3 = nil
  c4 = nil
  c5 = number 1
  c6 = number 1
  c7 = number 2
  c8 = number 3
  c9 = number 1
  c10 = number 1

names:
  n0 = "makeCounter#0"
  n1 = "start#1"
  n2 = "value#2"
  n3 = "next#3"
  n4 = "value#2"
  n5 = "value#2"
  n6 = "value#2"
  n7 = "next#3"
  n8 = "next#3"
  n9 = "makeCounter#0"
  n10 = "makeCounter#0"
  n11 = "counter#4"
  n12 = "counter#4"
  n13 = "xs#5"
  n14 = "xs#5"
  n15 = "xs#5"
  n16 = "xs#5"

main registers=19:
  r0 = constant c0
  store_var n0, r0
  r1 = make_function f1
  assign_var n9, r1
  r2 = load_var n10
  r3 = constant c5
  r4 = call r2 [r3]
  store_var n11, r4
  r5 = load_var n12
  r6 = call r5 []
  print r6
  r7 = constant c6
  r8 = constant c7
  r9 = constant c8
  r10 = array [r7, r8, r9]
  store_var n13, r10
  r11 = load_var n14
  r12 = constant c9
  r13 = load_var n15
  r14 = len r13
  r15 = assign_index r11, r12, r14
  r16 = load_var n16
  r17 = constant c10
  r18 = index r16, r17
  print r18

function f0 name="next" arity=0 registers=5:
  r0 = load_var n4
  r1 = constant c2
  r2 = add r0, r1
  assign_var n5, r2
  r3 = load_var n6
  return r3
  r4 = constant c3
  return r4

function f1 name="makeCounter" arity=1 registers=5:
  param 0 = "start"
  r0 = load_var n1
  store_var n2, r0
  r1 = constant c1
  store_var n3, r1
  r2 = make_function f0
  assign_var n7, r2
  r3 = load_var n8
  return r3
  r4 = constant c4
  return r4
CDBC
```

- [ ] **Step 5: Create artifact integration runner**

Run:

```bash
cat > tests/run_bytecode_artifact_tests.py <<'PY'
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


def discover_cases(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    return sorted(
        path for path in root.iterdir()
        if path.is_dir() and (path / "input.cd").is_file() and (path / "expected.cdbc").is_file()
    )


def check_case(compiler: Path, vm_manifest: Path, case_dir: Path) -> list[CheckResult]:
    source = case_dir / "input.cd"
    expected_path = case_dir / "expected.cdbc"
    expected = read_text(expected_path)
    results: list[CheckResult] = []

    with tempfile.TemporaryDirectory() as temp_dir:
        actual_path = Path(temp_dir) / "actual.cdbc"
        compile_command = [str(compiler), "--emit-bytecode", str(actual_path), str(source)]
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
        if not actual_path.is_file():
            results.append(CheckResult(compile_name, False, f"FAIL {compile_name} did not create {actual_path}"))
            return results

        actual = read_text(actual_path)
        if actual != expected:
            results.append(CheckResult(
                compile_name,
                False,
                f"FAIL {compile_name} artifact mismatch\n\n" + unified_diff(expected, actual, "expected", "actual"),
            ))
        else:
            results.append(CheckResult(compile_name, True))

        dump_command = ["cargo", "run", "--quiet", "--manifest-path", str(vm_manifest), "--", "dump", str(actual_path)]
        dumped = run_command(dump_command)
        dump_name = f"{case_dir.name} rust-dump"
        if dumped.returncode != 0:
            results.append(CheckResult(
                dump_name,
                False,
                f"FAIL {dump_name} exited with {dumped.returncode}\n\nSTDOUT:\n{dumped.stdout}\nSTDERR:\n{dumped.stderr}",
            ))
            return results
        if dumped.stderr:
            results.append(CheckResult(dump_name, False, f"FAIL {dump_name} produced unexpected stderr\n\n{dumped.stderr}"))
        if dumped.stdout != expected:
            results.append(CheckResult(
                dump_name,
                False,
                f"FAIL {dump_name} stdout mismatch\n\n" + unified_diff(expected, dumped.stdout, "expected", "actual"),
            ))
        else:
            results.append(CheckResult(dump_name, True))

    return results


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Compiler Design bytecode artifact integration tests.")
    parser.add_argument("compiler", type=Path, help="Path to compiler_design executable")
    parser.add_argument("vm", type=Path, help="Path to vm-rs directory or vm-rs/Cargo.toml")
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

    root = Path(__file__).resolve().parent / "bytecode_artifacts"
    cases = discover_cases(root)
    if not cases:
        print(f"no bytecode artifact fixtures found under {root}", file=sys.stderr)
        return 1

    results: list[CheckResult] = []
    for case_dir in cases:
        results.extend(check_case(compiler, vm_manifest, case_dir))

    failed = [result for result in results if not result.passed]
    for failure in failed:
        print(failure.message, file=sys.stderr)

    passed_count = len(results) - len(failed)
    print(f"bytecode artifact tests: {passed_count} passed, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
PY
chmod +x tests/run_bytecode_artifact_tests.py
```

- [ ] **Step 6: Run artifact tests and confirm they fail before implementation**

Run:

```bash
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: fails because `compiler_design` does not understand `--emit-bytecode` yet. The failure should mention a compile command exit with non-zero or missing artifact output.

- [ ] **Step 7: Commit failing artifact tests**

Run:

```bash
git add tests/run_bytecode_artifact_tests.py tests/bytecode_artifacts
git commit -m "test: cover bytecode artifact dump loop"
```

---

### Task 2: Add C++ `.cdbc` artifact emitter and CLI

**Files:**
- Create: `include/BytecodeTextEmitter.hpp`
- Create: `src/BytecodeTextEmitter.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/run_bytecode_artifact_tests.py` emit half fails only at Rust dump until Task 3

- [ ] **Step 1: Add emitter header**

Run:

```bash
cat > include/BytecodeTextEmitter.hpp <<'CPP'
#pragma once

#include "Bytecode.hpp"

#include <ostream>

void writeBytecodeText(std::ostream& out, const BytecodeProgram& program);
CPP
```

- [ ] **Step 2: Add emitter implementation**

Run:

```bash
cat > src/BytecodeTextEmitter.cpp <<'CPP'
#include "BytecodeTextEmitter.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string escapedString(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (char ch : value) {
        switch (ch) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    out << '"';
    return out.str();
}

std::string numberText(double value)
{
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
}

std::string constantText(const Value& value)
{
    switch (value.type()) {
    case Value::Type::Nil:
        return "nil";
    case Value::Type::Number:
        return "number " + numberText(value.asNumber());
    case Value::Type::Bool:
        return std::string("bool ") + (value.asBool() ? "true" : "false");
    case Value::Type::String:
        return "string " + escapedString(value.asString());
    case Value::Type::Function:
        throw std::runtime_error("cannot emit function value as bytecode constant");
    case Value::Type::Array:
        throw std::runtime_error("cannot emit array value as bytecode constant");
    }
    throw std::runtime_error("unsupported bytecode constant");
}

std::string reg(BytecodeRegister reg)
{
    return "r" + std::to_string(reg.index);
}

std::string constantRef(std::uint32_t index)
{
    return "c" + std::to_string(index);
}

std::string nameRef(std::uint32_t index)
{
    return "n" + std::to_string(index);
}

std::string functionRef(std::uint32_t index)
{
    return "f" + std::to_string(index);
}

void writeRegisterList(std::ostream& out, const std::vector<BytecodeRegister>& registers)
{
    out << '[';
    for (std::size_t i = 0; i < registers.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << reg(registers[i]);
    }
    out << ']';
}

BytecodeRegister requireDest(const BytecodeInstruction& instruction)
{
    if (!instruction.dest) {
        throw std::runtime_error("bytecode instruction missing destination");
    }
    return *instruction.dest;
}

BytecodeRegister requireLeft(const BytecodeInstruction& instruction)
{
    if (!instruction.left) {
        throw std::runtime_error("bytecode instruction missing left operand");
    }
    return *instruction.left;
}

BytecodeRegister requireRight(const BytecodeInstruction& instruction)
{
    if (!instruction.right) {
        throw std::runtime_error("bytecode instruction missing right operand");
    }
    return *instruction.right;
}

void writeInstruction(std::ostream& out, const BytecodeInstruction& instruction)
{
    out << "  ";
    switch (instruction.op) {
    case BytecodeOp::Constant:
        out << reg(requireDest(instruction)) << " = constant " << constantRef(instruction.operand);
        break;
    case BytecodeOp::MakeFunction:
        out << reg(requireDest(instruction)) << " = make_function " << functionRef(instruction.operand);
        break;
    case BytecodeOp::Array:
        out << reg(requireDest(instruction)) << " = array ";
        writeRegisterList(out, instruction.arguments);
        break;
    case BytecodeOp::Move:
        out << reg(requireDest(instruction)) << " = move " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::LoadVar:
        out << reg(requireDest(instruction)) << " = load_var " << nameRef(instruction.operand);
        break;
    case BytecodeOp::StoreVar:
        out << "store_var " << nameRef(instruction.operand) << ", " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::AssignVar:
        out << "assign_var " << nameRef(instruction.operand) << ", " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Call:
        out << reg(requireDest(instruction)) << " = call " << reg(requireLeft(instruction)) << ' ';
        writeRegisterList(out, instruction.arguments);
        break;
    case BytecodeOp::Index:
        out << reg(requireDest(instruction)) << " = index " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::AssignIndex:
        if (instruction.arguments.size() != 1) {
            throw std::runtime_error("assign_index expects one value operand");
        }
        out << reg(requireDest(instruction)) << " = assign_index " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction)) << ", " << reg(instruction.arguments.front());
        break;
    case BytecodeOp::Len:
        out << reg(requireDest(instruction)) << " = len " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Print:
        out << "print " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Return:
        out << "return " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Negate:
        out << reg(requireDest(instruction)) << " = negate " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Not:
        out << reg(requireDest(instruction)) << " = not " << reg(requireLeft(instruction));
        break;
    case BytecodeOp::Add:
        out << reg(requireDest(instruction)) << " = add " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Subtract:
        out << reg(requireDest(instruction)) << " = subtract " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Multiply:
        out << reg(requireDest(instruction)) << " = multiply " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Divide:
        out << reg(requireDest(instruction)) << " = divide " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Equal:
        out << reg(requireDest(instruction)) << " = equal " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::NotEqual:
        out << reg(requireDest(instruction)) << " = not_equal " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Greater:
        out << reg(requireDest(instruction)) << " = greater " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::GreaterEqual:
        out << reg(requireDest(instruction)) << " = greater_equal " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Less:
        out << reg(requireDest(instruction)) << " = less " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::LessEqual:
        out << reg(requireDest(instruction)) << " = less_equal " << reg(requireLeft(instruction)) << ", " << reg(requireRight(instruction));
        break;
    case BytecodeOp::Jump:
        out << "jump " << instruction.operand;
        break;
    case BytecodeOp::JumpIfFalse:
        out << "jump_if_false " << reg(requireLeft(instruction)) << ", " << instruction.operand;
        break;
    case BytecodeOp::JumpIfTrue:
        out << "jump_if_true " << reg(requireLeft(instruction)) << ", " << instruction.operand;
        break;
    }
    out << '\n';
}

void writeInstructions(std::ostream& out, const std::vector<BytecodeInstruction>& instructions)
{
    for (const BytecodeInstruction& instruction : instructions) {
        writeInstruction(out, instruction);
    }
}

} // namespace

void writeBytecodeText(std::ostream& out, const BytecodeProgram& program)
{
    out << "cdbc 0.1\n\n";

    out << "constants:\n";
    for (std::size_t i = 0; i < program.constants().size(); ++i) {
        out << "  " << constantRef(static_cast<std::uint32_t>(i)) << " = " << constantText(program.constants()[i]) << '\n';
    }

    out << "\nnames:\n";
    for (std::size_t i = 0; i < program.names().size(); ++i) {
        out << "  " << nameRef(static_cast<std::uint32_t>(i)) << " = " << escapedString(program.names()[i]) << '\n';
    }

    out << "\nmain registers=" << program.registerCount() << ":\n";
    writeInstructions(out, program.instructions());

    for (std::size_t i = 0; i < program.functions().size(); ++i) {
        const BytecodeFunction& function = program.functions()[i];
        out << "\nfunction " << functionRef(static_cast<std::uint32_t>(i))
            << " name=" << escapedString(function.name)
            << " arity=" << function.parameters.size()
            << " registers=" << function.registerCount << ":\n";
        for (std::size_t parameter = 0; parameter < function.parameters.size(); ++parameter) {
            out << "  param " << parameter << " = " << escapedString(function.parameters[parameter]) << '\n';
        }
        writeInstructions(out, function.instructions);
    }
}
CPP
```

- [ ] **Step 3: Add emitter source to CMake**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
path = Path('CMakeLists.txt')
text = path.read_text(encoding='utf-8')
text = text.replace('    src/Bytecode.cpp\n', '    src/Bytecode.cpp\n    src/BytecodeTextEmitter.cpp\n')
path.write_text(text, encoding='utf-8')
PY
```

- [ ] **Step 4: Add `--emit-bytecode` CLI support**

Edit `src/main.cpp`:

1. Add include:

```cpp
#include "BytecodeTextEmitter.hpp"
```

2. Add an output path variable near the other CLI state:

```cpp
std::optional<std::string> emitBytecodePath;
```

3. Update usage text to include the artifact form:

```cpp
std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [--run-bytecode] [file]\n"
          << "       " << executable << " --emit-bytecode output.cdbc file\n"
          << "If file is omitted, source is read from stdin except for --emit-bytecode, which requires a file.\n";
```

4. In argument parsing, add this branch before `--help`:

```cpp
} else if (arg == "--emit-bytecode") {
    if (i + 1 >= argc) {
        printUsage(argv[0]);
        return 64;
    }
    emitBytecodePath = argv[++i];
```

5. After argument parsing, reject missing input or combined output modes:

```cpp
if (emitBytecodePath) {
    if (inputPath.empty() || showTokens || showIr || showBytecode || runIr || runBytecode) {
        printUsage(argv[0]);
        return 64;
    }
}
```

6. Change AST/default mode condition to exclude artifact emission:

```cpp
if (!emitBytecodePath && !showIr && !showBytecode && !runIr && !runBytecode) {
    program.print(std::cout);
}
```

7. Change compile condition to include artifact emission:

```cpp
if (emitBytecodePath || showIr || showBytecode || runIr || runBytecode) {
```

8. Change bytecode construction condition:

```cpp
if (emitBytecodePath || showBytecode || runBytecode) {
```

9. After bytecode construction and before stdout sections, add artifact output:

```cpp
if (emitBytecodePath) {
    std::ostringstream artifact;
    writeBytecodeText(artifact, *bytecode);
    std::ofstream output(*emitBytecodePath);
    if (!output) {
        throw std::runtime_error("failed to open bytecode output file: " + *emitBytecodePath);
    }
    output << artifact.str();
    if (!output) {
        throw std::runtime_error("failed to write bytecode output file: " + *emitBytecodePath);
    }
    return 0;
}
```

- [ ] **Step 5: Build and run artifact tests after C++ emitter**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: C++ emit checks pass or get close enough to show exact fixture mismatches that should be fixed in the emitter/expected files. Rust dump checks still fail because `compiler-design-vm dump` is still the Phase 0 placeholder.

If the C++ artifact output differs only because real compiler numbering changed, inspect the diff and update fixture expected files only after confirming the output is canonical and semantically matches the fixture.

- [ ] **Step 6: Verify existing C++ behavior is unchanged**

Run:

```bash
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
rm -rf tests/__pycache__
```

Expected: existing tests pass.

- [ ] **Step 7: Commit C++ artifact emitter**

Run:

```bash
git add include/BytecodeTextEmitter.hpp src/BytecodeTextEmitter.cpp src/main.cpp CMakeLists.txt tests/bytecode_artifacts
git commit -m "feat: emit bytecode text artifacts"
```

---

### Task 3: Add Rust `.cdbc` parser, formatter, and `dump` command

**Files:**
- Create: `vm-rs/src/bytecode.rs`
- Create: `vm-rs/src/format.rs`
- Modify: `vm-rs/src/main.rs`
- Test: `cargo test --manifest-path vm-rs/Cargo.toml`
- Test: `tests/run_bytecode_artifact_tests.py`

- [ ] **Step 1: Add Rust artifact data structures**

Create `vm-rs/src/bytecode.rs` with these public structures:

```rust
#[derive(Clone, Debug, PartialEq)]
pub struct Program {
    pub constants: Vec<Constant>,
    pub names: Vec<String>,
    pub main: FunctionBody,
    pub functions: Vec<Function>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Constant {
    Nil,
    Number(String),
    Bool(bool),
    String(String),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Function {
    pub index: usize,
    pub name: String,
    pub arity: usize,
    pub registers: usize,
    pub params: Vec<String>,
    pub instructions: Vec<Instruction>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct FunctionBody {
    pub registers: usize,
    pub instructions: Vec<Instruction>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Instruction {
    Constant { dest: usize, constant: usize },
    MakeFunction { dest: usize, function: usize },
    Array { dest: usize, elements: Vec<usize> },
    Move { dest: usize, source: usize },
    LoadVar { dest: usize, name: usize },
    StoreVar { name: usize, value: usize },
    AssignVar { name: usize, value: usize },
    Call { dest: usize, callee: usize, arguments: Vec<usize> },
    Index { dest: usize, collection: usize, index: usize },
    AssignIndex { dest: usize, collection: usize, index: usize, value: usize },
    Len { dest: usize, value: usize },
    Print { value: usize },
    Return { value: usize },
    Negate { dest: usize, value: usize },
    Not { dest: usize, value: usize },
    Add { dest: usize, left: usize, right: usize },
    Subtract { dest: usize, left: usize, right: usize },
    Multiply { dest: usize, left: usize, right: usize },
    Divide { dest: usize, left: usize, right: usize },
    Equal { dest: usize, left: usize, right: usize },
    NotEqual { dest: usize, left: usize, right: usize },
    Greater { dest: usize, left: usize, right: usize },
    GreaterEqual { dest: usize, left: usize, right: usize },
    Less { dest: usize, left: usize, right: usize },
    LessEqual { dest: usize, left: usize, right: usize },
    Jump { target: usize },
    JumpIfFalse { condition: usize, target: usize },
    JumpIfTrue { condition: usize, target: usize },
}
```

- [ ] **Step 2: Add parser/formatter module**

Create `vm-rs/src/format.rs` with:

- `ParseError { line: usize, message: String }` implementing `Display`.
- `pub fn parse_program(source: &str) -> Result<Program, ParseError>`.
- `pub fn format_program(program: &Program) -> String`.
- helpers for:
  - string escaping/unescaping;
  - `rN`, `cN`, `nN`, `fN` references;
  - register lists `[r0, r1]` and `[]`;
  - dest instruction split around ` = `;
  - all opcode variants listed in `Instruction`.

Use a simple line parser with fixed sections:

```rust
struct Parser<'a> {
    lines: Vec<(usize, &'a str)>,
    current: usize,
}
```

Important parser behavior:

- Ignore blank lines between sections.
- Require first non-blank line to be `cdbc 0.1`.
- Require `constants:`, `names:`, and `main registers=N:` in order.
- Parse function sections after main until EOF.
- In function sections, parse `param` lines before instructions.
- Require parsed function param count to equal `arity`.
- Reject unknown opcodes with `unknown opcode `<name>``.
- Reject malformed references with messages like `expected register reference`.

Formatter behavior:

- Always prints canonical section order.
- Always ends output with a trailing newline.
- Emits `call` and `array` lists in `[r0, r1]` form.
- Emits function sections in vector order.

Add Rust unit tests in `format.rs`:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_minimal_program() { /* parse cdbc 0.1 with empty constants/names and one print */ }

    #[test]
    fn parses_and_formats_string_escapes() { /* includes \\, \", \n, \r, \t */ }

    #[test]
    fn rejects_bad_header() { /* expects line 1 parse error */ }

    #[test]
    fn rejects_unknown_opcode() { /* includes r0 = mystery r1 */ }

    #[test]
    fn parses_all_opcode_shapes() { /* one artifact with all Instruction variants */ }
}
```

- [ ] **Step 3: Update Rust CLI for `dump`**

Replace `vm-rs/src/main.rs` with:

```rust
mod bytecode;
mod format;

use std::env;
use std::fs;
use std::process;

const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm dump <program.cdbc>\n\
  compiler-design-vm run <program.cdbc>   (planned)\n\n\
Current phase: .cdbc parsing and canonical dump are implemented. Bytecode execution is not implemented in this phase.\n";

fn help_text() -> &'static str {
    HELP
}

fn dump(path: &str) -> Result<(), String> {
    let source = fs::read_to_string(path).map_err(|error| format!("error: failed to read `{}`: {}", path, error))?;
    let program = format::parse_program(&source).map_err(|error| format!("error: {}", error))?;
    print!("{}", format::format_program(&program));
    Ok(())
}

fn main() {
    let mut args = env::args().skip(1);
    match args.next().as_deref() {
        None | Some("-h") | Some("--help") => {
            print!("{}", help_text());
        }
        Some("dump") => {
            let Some(path) = args.next() else {
                eprintln!("error: dump expects <program.cdbc>");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            };
            if args.next().is_some() {
                eprintln!("error: dump expects exactly one input file");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            }
            if let Err(error) = dump(&path) {
                eprintln!("{}", error);
                process::exit(1);
            }
        }
        Some("run") => {
            eprintln!("error: command `run` is planned but not implemented in this phase");
            eprintln!();
            eprint!("{}", help_text());
            process::exit(64);
        }
        Some(command) => {
            eprintln!("error: unknown command `{}`", command);
            eprintln!();
            eprint!("{}", help_text());
            process::exit(64);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::help_text;

    #[test]
    fn help_mentions_dump_scope() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
        assert!(help.contains("canonical dump are implemented"));
        assert!(help.contains("Bytecode execution is not implemented"));
    }
}
```

- [ ] **Step 4: Run Rust tests**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: all Rust unit tests pass.

- [ ] **Step 5: Run integration artifact tests**

Run:

```bash
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: all emit and Rust dump checks pass.

- [ ] **Step 6: Commit Rust parser/dump**

Run:

```bash
git add vm-rs/src/main.rs vm-rs/src/bytecode.rs vm-rs/src/format.rs
git commit -m "feat: parse and dump bytecode artifacts"
```

---

### Task 4: Add artifact tests to CTest and update documentation

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `docs/bytecode-text-format.md`
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`
- Test: CTest and docs grep checks

- [ ] **Step 1: Add artifact integration runner to CTest**

In `CMakeLists.txt`, add after the existing `golden` test:

```cmake
add_test(
    NAME bytecode_artifacts
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_bytecode_artifact_tests.py $<TARGET_FILE:compiler_design> ${CMAKE_SOURCE_DIR}/vm-rs
)
```

- [ ] **Step 2: Update bytecode text format docs**

Edit `docs/bytecode-text-format.md` so it no longer says the compiler does not emit `.cdbc` or the Rust VM does not parse it. Replace the phase status paragraph with:

```markdown
This phase implements the text artifact format at the compiler/VM boundary. The C++ compiler can emit `.cdbc` files with `--emit-bytecode`, and the Rust VM can parse and canonicalize them with `dump`. Rust bytecode execution is still a future phase.
```

Also add the exact CLI examples:

```markdown
```sh
compiler_design --emit-bytecode output.cdbc input.cd
compiler-design-vm dump output.cdbc
```
```

- [ ] **Step 3: Update README**

Add this paragraph after the existing backend note:

```markdown
Bytecode artifacts can be emitted and inspected without executing them:

```sh
./build/compiler_design --emit-bytecode program.cdbc examples/hello.cd
cargo run --manifest-path vm-rs/Cargo.toml -- dump program.cdbc
```

The Rust VM `dump` command parses `.cdbc` and reprints canonical text. Rust bytecode execution remains a future phase.
```

Use a four-backtick outer fence if editing manually inside an existing Markdown code block.

- [ ] **Step 4: Update AGENTS**

In `AGENTS.md`, add `include/BytecodeTextEmitter.hpp` and `src/BytecodeTextEmitter.cpp` to the architecture map near bytecode files:

```markdown
- `include/BytecodeTextEmitter.hpp`, `src/BytecodeTextEmitter.cpp`: stable `.cdbc` text artifact emission for the Rust VM boundary.
```

Add this verification command to the command block:

```sh
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
cargo test --manifest-path vm-rs/Cargo.toml
```

Add a workflow note:

```markdown
When changing bytecode opcodes or artifact formatting, update `docs/bytecode-text-format.md`, the C++ `BytecodeTextEmitter`, Rust parser/formatter in `vm-rs/src/format.rs`, and `tests/bytecode_artifacts/` together.
```

- [ ] **Step 5: Update roadmap**

In `docs/roadmap.md`, update the backend track bullets:

```markdown
- Phase 0: rename to Compiler Design, scaffold `vm-rs/`, and document the planned `.cdbc` text format. Implemented.
- Phase 1: add a C++ `.cdbc` bytecode artifact emitter. Implemented.
- Phase 2: add Rust VM `.cdbc` parser and dump support. Implemented.
- Phase 3: add Rust VM executor parity for current bytecode semantics.
- Phase 4: explore GC heap ownership/root scanning, task scheduling, and JIT metadata/hot paths.
```

- [ ] **Step 6: Run documentation and CTest verification**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
cargo test --manifest-path vm-rs/Cargo.toml
rm -rf tests/__pycache__
```

Expected: all checks pass.

- [ ] **Step 7: Commit docs and CTest integration**

Run:

```bash
git add CMakeLists.txt docs/bytecode-text-format.md README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document bytecode artifact dump loop"
```

---

### Task 5: Full verification and branch completion

**Files:**
- Verify all source, Rust, tests, docs.

- [ ] **Step 1: Run full clean C++ and artifact verification**

Run:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/run_bytecode_artifact_tests.py ./build/compiler_design vm-rs
rm -rf tests/__pycache__
```

Expected:

- CMake configure/build passes.
- CTest passes, including `bytecode_artifacts`.
- Existing golden tests pass.
- Golden runner selftests pass.
- Artifact integration tests pass.

- [ ] **Step 2: Run Rust verification**

Run:

```bash
cargo test --manifest-path vm-rs/Cargo.toml
cargo run --manifest-path vm-rs/Cargo.toml -- dump tests/bytecode_artifacts/arithmetic/expected.cdbc
```

Expected:

- Rust tests pass.
- Dump output equals the canonical arithmetic artifact and exits 0.

- [ ] **Step 3: Verify non-goals**

Run:

```bash
grep -R "struct VM\|execute_program\|run_program" -n vm-rs/src || true
./build/compiler_design --run-bytecode tests/bytecode_artifacts/functions_arrays/input.cd
```

Expected:

- The grep command finds no Rust executor implementation.
- `--run-bytecode` still executes through the C++ VM and prints:

```text
2
3
```

- [ ] **Step 4: Review diff and status**

Run:

```bash
git diff --stat HEAD~5..HEAD
git diff --name-status HEAD~5..HEAD
git status --short --branch
```

Expected: diff includes only `.cdbc` emitter/parser/dump, artifact tests, docs, and plan/spec docs. Working tree is clean.

- [ ] **Step 5: Complete branch**

Report exact verification results. Then use `superpowers:finishing-a-development-branch` to present merge/push/keep/discard options.
