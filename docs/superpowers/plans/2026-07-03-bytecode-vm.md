# Bytecode VM Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a parallel register-based bytecode backend with `--bytecode` and `--run-bytecode`, preserving current IR interpreter behavior while creating VM boundaries for later GC, task scheduling, and JIT work.

**Architecture:** Keep the existing frontend and IR compiler as the semantic source of truth, then lower `IRProgram` into a separate `BytecodeProgram`. Execute bytecode in a new `BytecodeVM` with explicit `VMHeap`, `VMFrame`, and `VMThread` boundaries, but keep the existing `Value` representation and `shared_ptr` object storage in this phase.

**Tech Stack:** C++17, CMake, existing recursive-descent compiler pipeline, Python golden test runner.

---

## File Structure

Create:

- `include/Bytecode.hpp` — bytecode opcodes, instruction/function/program containers, printer declarations.
- `src/Bytecode.cpp` — bytecode opcode names, register formatting, deterministic printer.
- `include/BytecodeCompiler.hpp` — `IRProgram` to `BytecodeProgram` compiler API and compile error type.
- `src/BytecodeCompiler.cpp` — direct lowering from every current `IROp` to `BytecodeOp`.
- `include/BytecodeVM.hpp` — bytecode runtime error, VM heap, VM frame/thread state, VM API.
- `src/BytecodeVM.cpp` — bytecode interpreter implementation.
- `tests/golden/bytecode_smoke/input.cd` — small fixture for bytecode printing and execution.
- `tests/golden/bytecode_control_flow/input.cd` — control-flow fixture for `run_bytecode.out`.
- `tests/golden/bytecode_functions_closures/input.cd` — functions, recursion, and closure fixture for `run_bytecode.out`.
- `tests/golden/bytecode_arrays/input.cd` — arrays and indexing fixture for `run_bytecode.out`.
- `tests/golden/runtime_errors/bytecode_division_by_zero.cd` — runtime error fixture for bytecode execution.
- `tests/golden/runtime_errors/bytecode_array_index_out_of_range.cd` — runtime error fixture for bytecode execution.

Modify:

- `CMakeLists.txt` — compile new bytecode source files.
- `src/main.cpp` — add `--bytecode` and `--run-bytecode`.
- `tests/run_golden_tests.py` — add optional `bytecode.out`, `run_bytecode.out`, `.run_bytecode.err`, and `.run_bytecode.exit` support.
- `tests/run_golden_tests_selftest.py` — selftests for new runner support.
- `README.md` — document bytecode backend and CLI modes.
- `docs/roadmap.md` — mark Phase 8A as the active/implemented backend step when complete and list future GC/task/JIT follow-ups.
- `AGENTS.md` — add bytecode architecture map entries and verification expectations.

Reference:

- `docs/superpowers/specs/2026-07-03-bytecode-vm-design.md`
- `include/IR.hpp`
- `src/IR.cpp`
- `src/IRInterpreter.cpp`
- `include/Value.hpp`
- `src/Value.cpp`

---

### Task 1: Extend the Golden Runner for Bytecode Modes

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`

- [ ] **Step 1: Add failing selftests for bytecode success checks**

Append these tests before the final `if __name__ == "__main__":` block in `tests/run_golden_tests_selftest.py`:

```python
    def test_success_case_with_bytecode_expected_file_runs_bytecode_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "bytecode_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "bytecode.out").write_text("bytecode output\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--bytecode" not in sys.argv:
                        sys.stderr.write("missing --bytecode\\n")
                        raise SystemExit(1)
                    sys.stdout.write("bytecode output\\n")
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "bytecode_case --bytecode")

    def test_success_case_with_run_bytecode_expected_file_runs_run_bytecode_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "run_bytecode_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "run_bytecode.out").write_text("1\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--run-bytecode" not in sys.argv:
                        sys.stderr.write("missing --run-bytecode\\n")
                        raise SystemExit(1)
                    sys.stdout.write("1\\n")
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "run_bytecode_case --run-bytecode")

    def test_runtime_error_case_with_run_bytecode_expected_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "bytecode_error.cd").write_text("print 1 / 0;\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.run_bytecode.err").write_text("Runtime error: division by zero\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.run_bytecode.exit").write_text("1\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--run-bytecode" not in sys.argv:
                        sys.stderr.write("missing --run-bytecode\\n")
                        raise SystemExit(1)
                    sys.stderr.write("Runtime error: division by zero\\n")
                    raise SystemExit(1)
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "runtime_errors/bytecode_error --run-bytecode")
```

- [ ] **Step 2: Run selftests to verify they fail**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: failure because `SUCCESS_CHECKS` does not include `bytecode.out` / `run_bytecode.out`, and runtime bytecode error checks are not discovered.

- [ ] **Step 3: Add bytecode success checks**

In `tests/run_golden_tests.py`, replace `SUCCESS_CHECKS` with:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
    ("--run", ("--run",), "run.out"),
    ("--run-bytecode", ("--run-bytecode",), "run_bytecode.out"),
)
```

- [ ] **Step 4: Add runtime bytecode error support**

In `tests/run_golden_tests.py`, add this helper after `unexpected_runtime_stdout_result()`:

```python
def check_runtime_error_execution(
    compiler: Path,
    source: Path,
    update: bool,
    args: tuple[str, ...],
    err_suffix: str,
    exit_suffix: str,
    display_name: str,
) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(err_suffix)
    exit_path = stem.with_suffix(exit_suffix)
    case_name = f"runtime_errors/{source.stem} {display_name}"

    completed = run_compiler(compiler, args, source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_runtime_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_runtime_stdout_result(case_name, completed.stdout))

    if not err_path.exists():
        return []

    expected_err = read_text(err_path)
    actual_err = completed.stderr
    if actual_err != expected_err:
        diff = unified_diff(expected_err, actual_err, "expected stderr", "actual stderr")
        results.append(CheckResult(case_name, False, f"FAIL {case_name} stderr mismatch\n\n{diff}"))

    if not exit_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected exit file: {exit_path}"))
    else:
        expected_exit_text = read_text(exit_path).strip()
        actual_exit_text = str(completed.returncode)
        if actual_exit_text != expected_exit_text:
            results.append(
                CheckResult(
                    case_name,
                    False,
                    f"FAIL {case_name} exit code mismatch\nexpected: {expected_exit_text}\nactual: {actual_exit_text}",
                )
            )

    if not results:
        results.append(CheckResult(case_name, True))

    return results
```

Then replace `check_runtime_error_case()` with:

```python
def check_runtime_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    results = check_runtime_error_execution(
        compiler,
        source,
        update,
        ("--run",),
        ".run.err",
        ".exit",
        "--run",
    )
    bytecode_results = check_runtime_error_execution(
        compiler,
        source,
        update,
        ("--run-bytecode",),
        ".run_bytecode.err",
        ".run_bytecode.exit",
        "--run-bytecode",
    )
    results.extend(bytecode_results)
    return results
```

This keeps existing runtime fixtures unchanged: a bytecode runtime error check only runs in non-update mode when `.run_bytecode.err` exists.

- [ ] **Step 5: Run selftests to verify they pass**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: all selftests pass.

- [ ] **Step 6: Commit runner support**

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: support bytecode golden checks"
```

---

### Task 2: Add Bytecode Data Model, Printer, and IR Lowering

**Files:**
- Create: `include/Bytecode.hpp`
- Create: `src/Bytecode.cpp`
- Create: `include/BytecodeCompiler.hpp`
- Create: `src/BytecodeCompiler.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/golden/bytecode_smoke/input.cd`
- Test: `tests/golden/bytecode_smoke/bytecode.out`

- [ ] **Step 1: Add a failing bytecode print fixture**

Create `tests/golden/bytecode_smoke/input.cd`:

```text
print 1 + 2;
```

Create `tests/golden/bytecode_smoke/bytecode.out`:

```text
main registers=3
0000  b0 = constant #0 1
0001  b1 = constant #1 2
0002  b2 = add b0, b1
0003  print b2
```

- [ ] **Step 2: Run the failing golden subset**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: failure for `bytecode_smoke --bytecode` because the CLI flag does not exist.

- [ ] **Step 3: Create `include/Bytecode.hpp`**

Add:

```cpp
#pragma once

#include "Value.hpp"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct BytecodeRegister {
    std::uint32_t index = 0;
};

enum class BytecodeOp {
    Constant,
    MakeFunction,
    Array,
    Move,
    LoadVar,
    StoreVar,
    AssignVar,
    Call,
    Index,
    Print,
    Return,
    Negate,
    Not,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Jump,
    JumpIfFalse,
    JumpIfTrue,
};

struct BytecodeInstruction {
    BytecodeOp op;
    std::optional<BytecodeRegister> dest;
    std::optional<BytecodeRegister> left;
    std::optional<BytecodeRegister> right;
    std::vector<BytecodeRegister> arguments;
    std::uint32_t operand = 0;
};

struct BytecodeFunction {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<BytecodeInstruction> instructions;
    std::uint32_t registerCount = 0;
};

class BytecodeProgram {
public:
    void setConstants(std::vector<Value> constants);
    void setNames(std::vector<std::string> names);
    void setInstructions(std::vector<BytecodeInstruction> instructions);
    void setRegisterCount(std::uint32_t registerCount);
    void setFunctions(std::vector<BytecodeFunction> functions);

    const std::vector<Value>& constants() const;
    const std::vector<std::string>& names() const;
    const std::vector<BytecodeInstruction>& instructions() const;
    const std::vector<BytecodeFunction>& functions() const;
    std::uint32_t registerCount() const;

    void print(std::ostream& out) const;

private:
    std::vector<Value> constants_;
    std::vector<std::string> names_;
    std::vector<BytecodeInstruction> instructions_;
    std::uint32_t registerCount_ = 0;
    std::vector<BytecodeFunction> functions_;
};

std::string bytecodeOpName(BytecodeOp op);
std::ostream& operator<<(std::ostream& out, BytecodeRegister reg);
```

- [ ] **Step 4: Create `include/BytecodeCompiler.hpp`**

Add:

```cpp
#pragma once

#include "Bytecode.hpp"
#include "Diagnostic.hpp"
#include "IR.hpp"

#include <string>

class BytecodeCompileError final : public DiagnosticError {
public:
    explicit BytecodeCompileError(std::string message);
};

class BytecodeCompiler {
public:
    BytecodeProgram compile(const IRProgram& ir);

private:
    std::vector<BytecodeInstruction> lowerInstructions(const std::vector<IRInstruction>& instructions);
    BytecodeInstruction lowerInstruction(const IRInstruction& instruction);
    BytecodeFunction lowerFunction(const IRFunction& function);
};
```

- [ ] **Step 5: Create `src/Bytecode.cpp` with deterministic printing**

Implement the same formatting rules as the fixture:

```cpp
#include "Bytecode.hpp"

#include <iomanip>
#include <stdexcept>
#include <utility>

namespace {

bool isUnary(BytecodeOp op)
{
    return op == BytecodeOp::Negate || op == BytecodeOp::Not;
}

bool isBinary(BytecodeOp op)
{
    switch (op) {
    case BytecodeOp::Add:
    case BytecodeOp::Subtract:
    case BytecodeOp::Multiply:
    case BytecodeOp::Divide:
    case BytecodeOp::Equal:
    case BytecodeOp::NotEqual:
    case BytecodeOp::Greater:
    case BytecodeOp::GreaterEqual:
    case BytecodeOp::Less:
    case BytecodeOp::LessEqual:
        return true;
    default:
        return false;
    }
}

void printConstantOperand(std::ostream& out, const BytecodeProgram& program, std::uint32_t operand)
{
    out << " #" << operand;
    if (operand < program.constants().size()) {
        out << " " << program.constants()[operand];
    }
}

void printNameOperand(std::ostream& out, const BytecodeProgram& program, std::uint32_t operand)
{
    out << " @" << operand;
    if (operand < program.names().size()) {
        out << " " << program.names()[operand];
    }
}

void printInstruction(std::ostream& out, const BytecodeProgram& program, const BytecodeInstruction& instruction, std::size_t index)
{
    out << std::setw(4) << std::setfill('0') << index << std::setfill(' ') << "  ";
    if (instruction.dest) {
        out << *instruction.dest << " = ";
    }
    out << bytecodeOpName(instruction.op);

    if (instruction.op == BytecodeOp::Constant) {
        printConstantOperand(out, program, instruction.operand);
    } else if (instruction.op == BytecodeOp::MakeFunction) {
        out << " $" << instruction.operand;
        if (instruction.operand < program.functions().size()) {
            const BytecodeFunction& function = program.functions()[instruction.operand];
            out << " " << function.name << "/" << function.parameters.size();
        }
    } else if (instruction.op == BytecodeOp::Array) {
        out << " [";
        for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
            if (arg != 0) {
                out << ", ";
            }
            out << instruction.arguments[arg];
        }
        out << "]";
    } else if (instruction.op == BytecodeOp::Move) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (instruction.op == BytecodeOp::LoadVar) {
        printNameOperand(out, program, instruction.operand);
    } else if (instruction.op == BytecodeOp::StoreVar || instruction.op == BytecodeOp::AssignVar) {
        printNameOperand(out, program, instruction.operand);
        if (instruction.left) {
            out << ", " << *instruction.left;
        }
    } else if (instruction.op == BytecodeOp::Call) {
        if (instruction.left) {
            out << " " << *instruction.left << "(";
            for (std::size_t arg = 0; arg < instruction.arguments.size(); ++arg) {
                if (arg != 0) {
                    out << ", ";
                }
                out << instruction.arguments[arg];
            }
            out << ")";
        }
    } else if (instruction.op == BytecodeOp::Index) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
    } else if (instruction.op == BytecodeOp::Print || instruction.op == BytecodeOp::Return || isUnary(instruction.op)) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
    } else if (isBinary(instruction.op)) {
        if (instruction.left) {
            out << " " << *instruction.left;
        }
        if (instruction.right) {
            out << ", " << *instruction.right;
        }
    } else if (instruction.op == BytecodeOp::Jump) {
        out << " " << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
    } else if (instruction.op == BytecodeOp::JumpIfFalse || instruction.op == BytecodeOp::JumpIfTrue) {
        if (instruction.left) {
            out << " " << *instruction.left << ", ";
        } else {
            out << " ";
        }
        out << std::setw(4) << std::setfill('0') << instruction.operand << std::setfill(' ');
    }
    out << '\n';
}

} // namespace
```

Then add setters/getters, `bytecodeOpName()`, `operator<<`, and `BytecodeProgram::print()` below the anonymous namespace. `BytecodeProgram::print()` must print main first, then each function:

```cpp
void BytecodeProgram::print(std::ostream& out) const
{
    out << "main registers=" << registerCount_ << '\n';
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        printInstruction(out, *this, instructions_[i], i);
    }

    for (std::size_t functionIndex = 0; functionIndex < functions_.size(); ++functionIndex) {
        const BytecodeFunction& function = functions_[functionIndex];
        out << '\n';
        out << "function $" << functionIndex << " " << function.name << "/" << function.parameters.size()
            << " registers=" << function.registerCount << '\n';
        for (std::size_t i = 0; i < function.instructions.size(); ++i) {
            printInstruction(out, *this, function.instructions[i], i);
        }
    }
}
```

- [ ] **Step 6: Create `src/BytecodeCompiler.cpp`**

Implement direct lowering. The complete `lowerInstruction()` switch must map every current `IROp`:

```cpp
#include "BytecodeCompiler.hpp"

#include <limits>
#include <utility>

namespace {

BytecodeRegister lowerRegister(IRRegister reg)
{
    if (reg.index > std::numeric_limits<std::uint32_t>::max()) {
        throw BytecodeCompileError("register index out of range");
    }
    return BytecodeRegister{static_cast<std::uint32_t>(reg.index)};
}

std::optional<BytecodeRegister> lowerRegister(std::optional<IRRegister> reg)
{
    if (!reg) {
        return std::nullopt;
    }
    return lowerRegister(*reg);
}

std::vector<BytecodeRegister> lowerRegisters(const std::vector<IRRegister>& registers)
{
    std::vector<BytecodeRegister> lowered;
    lowered.reserve(registers.size());
    for (IRRegister reg : registers) {
        lowered.push_back(lowerRegister(reg));
    }
    return lowered;
}

std::uint32_t lowerOperand(std::size_t operand)
{
    if (operand > std::numeric_limits<std::uint32_t>::max()) {
        throw BytecodeCompileError("operand out of range");
    }
    return static_cast<std::uint32_t>(operand);
}

BytecodeOp lowerOp(IROp op)
{
    switch (op) {
    case IROp::Constant: return BytecodeOp::Constant;
    case IROp::MakeFunction: return BytecodeOp::MakeFunction;
    case IROp::Array: return BytecodeOp::Array;
    case IROp::Copy: return BytecodeOp::Move;
    case IROp::LoadVar: return BytecodeOp::LoadVar;
    case IROp::StoreVar: return BytecodeOp::StoreVar;
    case IROp::AssignVar: return BytecodeOp::AssignVar;
    case IROp::Call: return BytecodeOp::Call;
    case IROp::Index: return BytecodeOp::Index;
    case IROp::Print: return BytecodeOp::Print;
    case IROp::Return: return BytecodeOp::Return;
    case IROp::Negate: return BytecodeOp::Negate;
    case IROp::Not: return BytecodeOp::Not;
    case IROp::Add: return BytecodeOp::Add;
    case IROp::Subtract: return BytecodeOp::Subtract;
    case IROp::Multiply: return BytecodeOp::Multiply;
    case IROp::Divide: return BytecodeOp::Divide;
    case IROp::Equal: return BytecodeOp::Equal;
    case IROp::NotEqual: return BytecodeOp::NotEqual;
    case IROp::Greater: return BytecodeOp::Greater;
    case IROp::GreaterEqual: return BytecodeOp::GreaterEqual;
    case IROp::Less: return BytecodeOp::Less;
    case IROp::LessEqual: return BytecodeOp::LessEqual;
    case IROp::Jump: return BytecodeOp::Jump;
    case IROp::JumpIfFalse: return BytecodeOp::JumpIfFalse;
    case IROp::JumpIfTrue: return BytecodeOp::JumpIfTrue;
    }
    throw BytecodeCompileError("unsupported IR opcode");
}

} // namespace
```

Then implement `BytecodeCompileError`, `compile()`, `lowerInstructions()`, `lowerInstruction()`, and `lowerFunction()` so constants, names, register count, functions, instruction operands, and jump targets are copied exactly.

- [ ] **Step 7: Add bytecode sources to CMake**

In `CMakeLists.txt`, add:

```cmake
    src/Bytecode.cpp
    src/BytecodeCompiler.cpp
```

near the other source files in `add_executable(compiler_demo ...)`.

- [ ] **Step 8: Build to catch compile errors**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: build succeeds. The bytecode golden still fails because `--bytecode` is not wired into `main.cpp`.

- [ ] **Step 9: Commit bytecode model and compiler**

```bash
git add CMakeLists.txt include/Bytecode.hpp src/Bytecode.cpp include/BytecodeCompiler.hpp src/BytecodeCompiler.cpp tests/golden/bytecode_smoke
git commit -m "feat: add bytecode program lowering"
```

---

### Task 3: Add `--bytecode` CLI Printing

**Files:**
- Modify: `src/main.cpp`
- Test: `tests/golden/bytecode_smoke/bytecode.out`

- [ ] **Step 1: Include bytecode headers**

At the top of `src/main.cpp`, add:

```cpp
#include "BytecodeCompiler.hpp"
```

- [ ] **Step 2: Add CLI flags**

In `main()`, add:

```cpp
    bool showBytecode = false;
```

Then add argument parsing:

```cpp
        } else if (arg == "--bytecode") {
            showBytecode = true;
```

Update usage:

```cpp
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--run] [--run-bytecode] [file]\n"
              << "If file is omitted, source is read from stdin.\n";
```

Do not implement `--run-bytecode` in this task. Add the flag to usage now because the next task wires execution.

- [ ] **Step 3: Compile and print bytecode**

Replace:

```cpp
        if (!showIr && !runIr) {
            program.print(std::cout);
        }
```

with:

```cpp
        if (!showIr && !showBytecode && !runIr) {
            program.print(std::cout);
        }
```

Then replace the existing `if (showIr || runIr) { ... }` block with logic that builds IR when any backend output needs it:

```cpp
        if (showIr || showBytecode || runIr) {
            IRCompiler compiler;
            IRProgram ir = compiler.compile(program, resolvedNames);

            if (showIr) {
                ir.print(std::cout);
                if (showBytecode || runIr) {
                    std::cout << '\n';
                }
            }

            if (showBytecode) {
                BytecodeCompiler bytecodeCompiler;
                BytecodeProgram bytecode = bytecodeCompiler.compile(ir);
                bytecode.print(std::cout);
                if (runIr) {
                    std::cout << '\n';
                }
            }

            if (runIr) {
                IRInterpreter interpreter(std::cout);
                interpreter.execute(ir);
            }
        }
```

- [ ] **Step 4: Run the bytecode smoke golden**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `bytecode_smoke --bytecode` passes if the expected print format matches implementation; other tests still pass except any fixtures needing `--run-bytecode`, which have not been added yet.

- [ ] **Step 5: Commit bytecode CLI printing**

```bash
git add src/main.cpp tests/golden/bytecode_smoke/bytecode.out
git commit -m "feat: print bytecode from cli"
```

---

### Task 4: Add Bytecode VM Core for Literals, Arithmetic, Jumps, and Printing

**Files:**
- Create: `include/BytecodeVM.hpp`
- Create: `src/BytecodeVM.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Test: `tests/golden/bytecode_smoke/run_bytecode.out`
- Test: `tests/golden/bytecode_control_flow/input.cd`
- Test: `tests/golden/bytecode_control_flow/run_bytecode.out`

- [ ] **Step 1: Add failing run-bytecode fixtures**

Create `tests/golden/bytecode_smoke/run_bytecode.out`:

```text
3
```

Create `tests/golden/bytecode_control_flow/input.cd`:

```text
let i = 0;
let total = 0;
while i < 4 {
  total = total + i;
  i = i + 1;
}

if total == 6 && false || true {
  print total;
} else {
  print 999;
}
```

Create `tests/golden/bytecode_control_flow/run_bytecode.out`:

```text
6
```

- [ ] **Step 2: Run golden tests to verify failure**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: failures for `--run-bytecode` because the CLI flag is not implemented.

- [ ] **Step 3: Create `include/BytecodeVM.hpp`**

Add:

```cpp
#pragma once

#include "Bytecode.hpp"
#include "Diagnostic.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

class BytecodeRuntimeError final : public DiagnosticError {
public:
    explicit BytecodeRuntimeError(std::string message);
};

class VMHeap {
public:
    Value makeFunction(std::string name, std::size_t functionIndex, std::size_t arity, std::size_t identity, std::shared_ptr<Environment> closure);
    Value makeArray(std::size_t identity, std::shared_ptr<std::vector<Value>> elements);
};

class BytecodeVM {
public:
    explicit BytecodeVM(std::ostream& output);

    void execute(const BytecodeProgram& program);
    const std::unordered_map<std::string, Value>& globals() const;

private:
    struct VMFrame {
        std::uint32_t functionIndex = 0;
        std::size_t ip = 0;
        std::vector<Value> registers;
        std::shared_ptr<Environment> locals = std::make_shared<Environment>();
        std::shared_ptr<Environment> closure = std::make_shared<Environment>();
        bool isMain = false;
    };

    struct VMThread {
        std::vector<VMFrame> frames;
        bool halted = false;
        Value returnValue = Value::nil();
    };

    struct ExecutionResult {
        bool returned = false;
        Value value = Value::nil();
    };

    Value readConstant(const BytecodeProgram& program, std::uint32_t index) const;
    std::string readName(const BytecodeProgram& program, std::uint32_t index) const;
    BytecodeRegister readDest(const BytecodeInstruction& instruction) const;
    BytecodeRegister readLeft(const BytecodeInstruction& instruction) const;
    BytecodeRegister readRight(const BytecodeInstruction& instruction) const;
    ExecutionResult executeInstructions(const BytecodeProgram& program, const std::vector<BytecodeInstruction>& instructions, VMFrame& frame);
    const Value& readRegister(const VMFrame& frame, BytecodeRegister reg) const;
    void writeRegister(VMFrame& frame, BytecodeRegister reg, Value value);
    void refreshGlobalsView() const;

    Value executeUnaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister value, Value (*operation)(double));
    Value executeBinaryNumber(const VMFrame& frame, const std::string& opName, BytecodeRegister left, BytecodeRegister right, Value (*operation)(double, double));
    Value executeAdd(const VMFrame& frame, BytecodeRegister left, BytecodeRegister right);

    std::ostream& output_;
    VMHeap heap_;
    VMThread mainThread_;
    std::shared_ptr<Environment> globals_ = std::make_shared<Environment>();
    mutable std::unordered_map<std::string, Value> globalsView_;
    std::size_t nextFunctionIdentity_ = 1;
    std::size_t nextArrayIdentity_ = 1;
};
```

- [ ] **Step 4: Create `src/BytecodeVM.cpp` core**

Implement:

- `BytecodeRuntimeError` with `DiagnosticKind::Runtime`.
- `VMHeap::makeFunction()` and `VMHeap::makeArray()` using existing `Value` factories.
- `execute()` reset state and run main instructions.
- register/constant/name validation helpers.
- arithmetic helpers copied in behavior from `IRInterpreter`.
- opcodes: `Constant`, `Move`, `Print`, `Return`, `Negate`, `Not`, `Add`, `Subtract`, `Multiply`, `Divide`, `Equal`, `NotEqual`, `Greater`, `GreaterEqual`, `Less`, `LessEqual`, `Jump`, `JumpIfFalse`, `JumpIfTrue`.

Use these exact runtime messages for helper validation:

```text
constant index out of range
name index out of range
register index out of range
jump target out of range
negate expects number, got <type>
subtract expects numbers
multiply expects numbers
divide expects numbers
greater expects numbers
greater_equal expects numbers
less expects numbers
less_equal expects numbers
division by zero
add expects two numbers or two strings
```

- [ ] **Step 5: Add bytecode VM source to CMake**

In `CMakeLists.txt`, add:

```cmake
    src/BytecodeVM.cpp
```

- [ ] **Step 6: Wire `--run-bytecode` in `main.cpp`**

Add:

```cpp
#include "BytecodeVM.hpp"
```

Add a flag:

```cpp
    bool runBytecode = false;
```

Parse:

```cpp
        } else if (arg == "--run-bytecode") {
            runBytecode = true;
```

Update default AST condition:

```cpp
        if (!showIr && !showBytecode && !runIr && !runBytecode) {
            program.print(std::cout);
        }
```

Update backend block condition:

```cpp
        if (showIr || showBytecode || runIr || runBytecode) {
```

Inside that block, build bytecode once when needed:

```cpp
            std::optional<BytecodeProgram> bytecode;
            if (showBytecode || runBytecode) {
                BytecodeCompiler bytecodeCompiler;
                bytecode = bytecodeCompiler.compile(ir);
            }
```

Print and run in the stable order:

```cpp
            if (showBytecode) {
                bytecode->print(std::cout);
                if (runIr || runBytecode) {
                    std::cout << '\n';
                }
            }

            if (runIr) {
                IRInterpreter interpreter(std::cout);
                interpreter.execute(ir);
                if (runBytecode) {
                    std::cout << '\n';
                }
            }

            if (runBytecode) {
                BytecodeVM vm(std::cout);
                vm.execute(*bytecode);
            }
```

Add `<optional>` include if needed.

- [ ] **Step 7: Run focused tests**

Run:

```bash
cmake -S . -B build
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: bytecode smoke and control-flow run fixtures pass. Function, closure, and array bytecode fixtures do not exist yet.

- [ ] **Step 8: Commit VM core**

```bash
git add CMakeLists.txt include/BytecodeVM.hpp src/BytecodeVM.cpp src/main.cpp tests/golden/bytecode_smoke tests/golden/bytecode_control_flow
git commit -m "feat: execute core bytecode"
```

---

### Task 5: Add Bytecode VM Variables, Environments, and Assignment

**Files:**
- Modify: `include/BytecodeVM.hpp`
- Modify: `src/BytecodeVM.cpp`
- Test: `tests/golden/bytecode_variables/input.cd`
- Test: `tests/golden/bytecode_variables/run_bytecode.out`

- [ ] **Step 1: Add failing variable fixture**

Create `tests/golden/bytecode_variables/input.cd`:

```text
let x = 1;
{
  let x = 10;
  x = x + 5;
  print x;
}
x = x + 2;
print x;
```

Create `tests/golden/bytecode_variables/run_bytecode.out`:

```text
15
3
```

- [ ] **Step 2: Run golden tests to verify failure**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: failure with an unsupported `store_var`, `load_var`, or `assign_var` bytecode operation.

- [ ] **Step 3: Add environment helpers to `include/BytecodeVM.hpp`**

Add private methods:

```cpp
    std::shared_ptr<Cell> findCell(const VMFrame& frame, const std::string& name) const;
    Value loadVariable(const VMFrame& frame, const std::string& name) const;
    void storeVariable(VMFrame& frame, const std::string& name, Value value);
    void assignVariable(VMFrame& frame, const std::string& name, Value value);
    std::shared_ptr<Environment> captureEnvironment(const VMFrame& frame) const;
```

- [ ] **Step 4: Implement variable helpers and opcodes**

In `src/BytecodeVM.cpp`, implement helpers with the same lookup order as `IRInterpreter`:

```text
locals -> closure -> globals
```

Use the exact undefined variable message:

```text
undefined variable `<name>`
```

Implement opcodes:

- `LoadVar`: read name from `operand`, write loaded value to `dest`.
- `StoreVar`: create a new `Cell`; store in `globals_` for `frame.isMain`, otherwise in `frame.locals`.
- `AssignVar`: find the nearest existing cell and mutate its `value`.

- [ ] **Step 5: Run golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `bytecode_variables --run-bytecode` passes.

- [ ] **Step 6: Commit variables support**

```bash
git add include/BytecodeVM.hpp src/BytecodeVM.cpp tests/golden/bytecode_variables
git commit -m "feat: execute bytecode variables"
```

---

### Task 6: Add Bytecode Functions, Calls, Returns, Recursion, and Closures

**Files:**
- Modify: `include/BytecodeVM.hpp`
- Modify: `src/BytecodeVM.cpp`
- Test: `tests/golden/bytecode_functions_closures/input.cd`
- Test: `tests/golden/bytecode_functions_closures/run_bytecode.out`

- [ ] **Step 1: Add failing function and closure fixture**

Create `tests/golden/bytecode_functions_closures/input.cd`:

```text
fun fib(n) {
  if n <= 1 {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

fun makeCounter() {
  let count = 0;
  fun next() {
    count = count + 1;
    return count;
  }
  return next;
}

print fib(6);
let counter = makeCounter();
print counter();
print counter();
```

Create `tests/golden/bytecode_functions_closures/run_bytecode.out`:

```text
8
1
2
```

- [ ] **Step 2: Run golden tests to verify failure**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: failure with unsupported `make_function` or `call`.

- [ ] **Step 3: Add call helper declaration**

In `include/BytecodeVM.hpp`, add:

```cpp
    Value callFunction(const BytecodeProgram& program, const FunctionValue& function, const std::vector<Value>& arguments);
```

- [ ] **Step 4: Implement `MakeFunction`, `Call`, and `callFunction()`**

In `src/BytecodeVM.cpp`:

- `MakeFunction` validates `operand < program.functions().size()`.
- `MakeFunction` writes `heap_.makeFunction(function.name, instruction.operand, function.parameters.size(), nextFunctionIdentity_++, captureEnvironment(frame))`.
- `Call` validates callee type is `Value::Type::Function`; otherwise throw:

```text
can only call functions
```

- `Call` reads arguments left-to-right from bytecode registers.
- `callFunction()` validates function index and arity. Use the exact arity message:

```text
expected <expected> arguments but got <actual>
```

- `callFunction()` creates a new `VMFrame`:
  - `isMain = false`
  - `closure = function.closure` or empty environment
  - `registers.assign(bytecodeFunction.registerCount, Value::nil())`
  - parameter cells in `frame.locals`
- `callFunction()` executes the target function's bytecode and returns the returned value or `nil`.

- [ ] **Step 5: Verify closure capture by reference**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `bytecode_functions_closures --run-bytecode` passes and prints `8`, `1`, `2`.

- [ ] **Step 6: Commit function support**

```bash
git add include/BytecodeVM.hpp src/BytecodeVM.cpp tests/golden/bytecode_functions_closures
git commit -m "feat: execute bytecode functions and closures"
```

---

### Task 7: Add Bytecode Arrays, Indexing, and Runtime Error Goldens

**Files:**
- Modify: `include/BytecodeVM.hpp`
- Modify: `src/BytecodeVM.cpp`
- Test: `tests/golden/bytecode_arrays/input.cd`
- Test: `tests/golden/bytecode_arrays/run_bytecode.out`
- Test: `tests/golden/runtime_errors/bytecode_division_by_zero.cd`
- Test: `tests/golden/runtime_errors/bytecode_division_by_zero.run_bytecode.err`
- Test: `tests/golden/runtime_errors/bytecode_division_by_zero.run_bytecode.exit`
- Test: `tests/golden/runtime_errors/bytecode_array_index_out_of_range.cd`
- Test: `tests/golden/runtime_errors/bytecode_array_index_out_of_range.run_bytecode.err`
- Test: `tests/golden/runtime_errors/bytecode_array_index_out_of_range.run_bytecode.exit`

- [ ] **Step 1: Add failing array fixture**

Create `tests/golden/bytecode_arrays/input.cd`:

```text
fun makeNested() {
  return [[1, 2], [3, 4]];
}

let xs = [10, "twenty", true, nil];
print xs[0];
print xs[1];
print makeNested()[1][0];
```

Create `tests/golden/bytecode_arrays/run_bytecode.out`:

```text
10
twenty
3
```

- [ ] **Step 2: Add bytecode runtime error fixtures**

Create `tests/golden/runtime_errors/bytecode_division_by_zero.cd`:

```text
print 1 / 0;
```

Create `tests/golden/runtime_errors/bytecode_division_by_zero.run_bytecode.err`:

```text
Runtime error: division by zero
```

Create `tests/golden/runtime_errors/bytecode_division_by_zero.run_bytecode.exit`:

```text
1
```

Create `tests/golden/runtime_errors/bytecode_array_index_out_of_range.cd`:

```text
let xs = [1];
print xs[1];
```

Create `tests/golden/runtime_errors/bytecode_array_index_out_of_range.run_bytecode.err`:

```text
Runtime error: array index out of range
```

Create `tests/golden/runtime_errors/bytecode_array_index_out_of_range.run_bytecode.exit`:

```text
1
```

- [ ] **Step 3: Run golden tests to verify array failure**

Run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: `bytecode_arrays --run-bytecode` fails with unsupported `array` or `index`. The division-by-zero runtime fixture may already pass if Task 4 implemented `Divide`.

- [ ] **Step 4: Add array/index helper declarations**

In `include/BytecodeVM.hpp`, add:

```cpp
    Value executeArray(const BytecodeInstruction& instruction, const VMFrame& frame);
    Value executeIndex(const VMFrame& frame, BytecodeRegister collection, BytecodeRegister index);
```

- [ ] **Step 5: Implement `Array` and `Index` opcodes**

In `src/BytecodeVM.cpp`:

- `Array`: read each argument register into a new `std::vector<Value>`, then call `heap_.makeArray(nextArrayIdentity_++, elements)`.
- `Index`: match `IRInterpreter::executeIndex()` behavior exactly.

Use exact messages:

```text
can only index arrays
array index must be number
array index must be integer
array index out of range
```

Use `std::trunc()` for integer validation and include `<cmath>`.

- [ ] **Step 6: Run focused and full golden tests**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_demo
```

Expected: all golden tests pass, including bytecode arrays and bytecode runtime error fixtures.

- [ ] **Step 7: Commit arrays and runtime errors**

```bash
git add include/BytecodeVM.hpp src/BytecodeVM.cpp tests/golden/bytecode_arrays tests/golden/runtime_errors/bytecode_division_by_zero.* tests/golden/runtime_errors/bytecode_array_index_out_of_range.*
git commit -m "feat: execute bytecode arrays"
```

---

### Task 8: Documentation, Roadmap, AGENTS, and Final Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README architecture list**

In `README.md`, update the feature list near the top so it includes:

```markdown
- Bytecode compiler: lowers register IR into a bytecode program.
- Bytecode VM: executes the bytecode backend in parallel with the IR interpreter.
```

- [ ] **Step 2: Update README run commands**

In `README.md`, add:

```sh
./build/compiler_demo --bytecode examples/hello.cd
./build/compiler_demo --run-bytecode examples/hello.cd
```

Add a concise note:

```markdown
`--run` executes the existing IR interpreter. `--run-bytecode` executes the newer bytecode VM. They are expected to match for implemented language features.
```

- [ ] **Step 3: Update roadmap Phase 8**

In `docs/roadmap.md`, replace the Phase 8 section with:

```markdown
## Phase 8: Bytecode VM or Backend Expansion

### Phase 8A: Bytecode VM — Implemented

Status: implemented. The compiler now has a parallel register-based bytecode backend. `--bytecode` prints lowered bytecode, and `--run-bytecode` executes it through the bytecode VM while preserving the existing `--ir` and `--run` paths.

The bytecode VM currently reuses the existing runtime `Value` representation. It includes VM heap and VM thread/frame boundaries so later phases can add GC, task scheduling, and JIT support without reworking the frontend.

Future Phase 8 follow-ups:

- GC: replace VM heap internals with tracing collection and explicit root scanning.
- Task scheduling: make VM threads schedulable with instruction budgets, yield points, and blocked states.
- JIT: compile hot `BytecodeFunction` units to native code using bytecode-level metadata.
```

- [ ] **Step 4: Update AGENTS architecture and workflow notes**

In `AGENTS.md`, add architecture map bullets:

```markdown
- `include/Bytecode.hpp`, `src/Bytecode.cpp`: bytecode opcodes, program/function containers, and bytecode printer output.
- `include/BytecodeCompiler.hpp`, `src/BytecodeCompiler.cpp`: IR-to-bytecode lowering.
- `include/BytecodeVM.hpp`, `src/BytecodeVM.cpp`: bytecode VM execution, VM heap boundary, and VM thread/frame state.
```

Add backend workflow bullets:

```markdown
When IR behavior changes, update both the IR interpreter and the bytecode backend unless the change is intentionally IR-only. Bytecode lowering should preserve current IR semantics, and `--run-bytecode` should match `--run` for supported programs.
```

Add golden convention bullets:

```markdown
Successful fixtures may include `bytecode.out` for `--bytecode` and `run_bytecode.out` for `--run-bytecode`. Runtime-error fixtures may include `.run_bytecode.err` and `.run_bytecode.exit` to check bytecode VM runtime diagnostics.
```

- [ ] **Step 5: Run full verification**

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

- `ctest` reports all tests passed.
- Golden runner reports all checks passed.
- Golden runner selftests report OK.
- `tests/__pycache__` is removed.

- [ ] **Step 6: Check git status**

Run:

```bash
git status --short
```

Expected: only intentional documentation changes are unstaged.

- [ ] **Step 7: Commit documentation**

```bash
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document bytecode vm backend"
```

---

## Self-Review Checklist

- Spec coverage:
  - Parallel backend: Tasks 2, 3, 4.
  - `--bytecode`: Tasks 2 and 3.
  - `--run-bytecode`: Tasks 4 through 7.
  - Golden support: Task 1.
  - Runtime diagnostics compatibility: Tasks 4 and 7.
  - VMHeap boundary: Task 4.
  - VMThread/VMFrame boundary: Task 4.
  - Future GC/task/JIT documentation: Task 8.
- No language grammar changes are planned.
- `docs/language-grammar.ebnf` is intentionally untouched.
- Each task has a focused commit.
- Full verification command is included in Task 8.

