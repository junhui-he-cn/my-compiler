# Module Interface Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `compiler_design --module-interface` to print stable frontend metadata for each loaded module's exported public API.

**Architecture:** Introduce a read-only `ModuleInterface` data model and a focused text emitter. After `TypeChecker::check()` succeeds, `TypeChecker::moduleInterfaces()` snapshots public exports from `ModuleSymbols` and `ModuleStmt`; `main.cpp` prints that snapshot in the new CLI mode without running IR or bytecode.

**Tech Stack:** C++17, existing recursive-descent compiler frontend, CMake/CTest, Python golden/CLI tests.

---

## File Structure

Create:

- `include/ModuleInterface.hpp` — plain data structs for module-interface snapshots.
- `include/ModuleInterfaceEmitter.hpp` — declaration for stable text output.
- `src/ModuleInterfaceEmitter.cpp` — text emitter implementation using `typeInfoName`.
- `tests/module_interface_emitter_tests.cpp` — focused C++ tests for sorting-independent emitter formatting and type rendering.

Modify:

- `include/TypeChecker.hpp` — include `ModuleInterface.hpp`; expose `const std::vector<ModuleInterface>& moduleInterfaces() const`; store `moduleInterfaces_`; declare private `buildModuleInterfaces(const Program&)`.
- `src/TypeChecker.cpp` — clear/build interface snapshots after successful type checking by copying public exports from `ModuleSymbols`.
- `src/main.cpp` — parse `--module-interface`, update usage, emit interface text after type checking, reject incompatible `--emit-bytecode` combinations.
- `CMakeLists.txt` — add emitter source to `compiler_design`; add `module_interface_emitter_tests` target.
- `tests/run_golden_tests.py` — add `module-interface.out` as an optional success output mode.
- `tests/run_golden_tests_selftest.py` — update tests that know the success-mode list and add coverage for the new mode where needed.
- `tests/cli_multi_source_tests.py` — add `--module-interface` coverage for import search paths.
- `tests/golden/module_interface_exports/` — new success fixture for values, structs, methods, private declarations, namespace imports, and re-export final exports.
- `README.md` — document `--module-interface` as a debug/introspection mode.

Do not modify:

- `docs/language-grammar.ebnf` — no syntax changes.
- IR, bytecode, `.cdbc`, or Rust VM files — no backend behavior changes.

---

### Task 1: Add focused emitter tests first

**Files:**
- Create: `tests/module_interface_emitter_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing C++ tests**

Create `tests/module_interface_emitter_tests.cpp` with this content:

```cpp
#include "ModuleInterface.hpp"
#include "ModuleInterfaceEmitter.hpp"
#include "TypeUtils.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectEqual(const std::string& name, const std::string& actual, const std::string& expected)
{
    if (actual == expected) {
        return;
    }
    ++failures;
    std::cerr << "FAIL " << name << "\nexpected:\n" << expected << "\nactual:\n" << actual << "\n";
}

std::string emit(const std::vector<ModuleInterface>& interfaces)
{
    std::ostringstream out;
    writeModuleInterfaceText(out, interfaces);
    return out.str();
}

void testEmptyAndSortedModules()
{
    std::vector<ModuleInterface> interfaces;

    ModuleInterface later;
    later.moduleId = 2;
    later.path = "later.cd";

    ModuleInterface entry;
    entry.moduleId = 0;
    entry.path = "main.cd";
    entry.isEntry = true;

    interfaces.push_back(later);
    interfaces.push_back(entry);

    expectEqual(
        "empty modules sorted by id",
        emit(interfaces),
        "module 0 entry \"main.cd\"\n"
        "\n"
        "module 2 \"later.cd\"\n");
}

void testValuesStructsFieldsMethodsAndTypes()
{
    ModuleInterface module;
    module.moduleId = 0;
    module.path = "main.cd";
    module.isEntry = true;

    module.values.push_back(ModuleInterfaceValue{"zeta", nullableType(namedStructType("Point"))});
    module.values.push_back(ModuleInterfaceValue{
        "make",
        functionType(
            std::vector<TypeInfo>{simpleType(StaticType::Number), arrayType(nullableType(simpleType(StaticType::String)))},
            namedStructType("Point"))});
    module.values.push_back(ModuleInterfaceValue{"dynamic", unknownType()});

    ModuleInterfaceStruct point;
    point.name = "Point";
    point.fields.push_back(ModuleInterfaceField{"y", simpleType(StaticType::Number)});
    point.fields.push_back(ModuleInterfaceField{"x", nullableType(simpleType(StaticType::Number))});
    point.methods.push_back(ModuleInterfaceMethod{"translate", {simpleType(StaticType::Number), simpleType(StaticType::Number)}, namedStructType("Point")});
    point.methods.push_back(ModuleInterfaceMethod{"length", {}, simpleType(StaticType::Number)});

    ModuleInterfaceStruct box;
    box.name = "Box";
    box.fields.push_back(ModuleInterfaceField{"items", arrayType(namedStructType("Point"))});

    module.structs.push_back(point);
    module.structs.push_back(box);

    expectEqual(
        "exports sorted and fields preserved",
        emit(std::vector<ModuleInterface>{module}),
        "module 0 entry \"main.cd\"\n"
        "  export value dynamic: unknown\n"
        "  export value make: fun(number, [string?]): Point\n"
        "  export value zeta: Point?\n"
        "  export struct Box\n"
        "    field items: [Point]\n"
        "  export struct Point\n"
        "    field y: number\n"
        "    field x: number?\n"
        "    method length(): number\n"
        "    method translate(number, number): Point\n");
}

} // namespace

int main()
{
    testEmptyAndSortedModules();
    testValuesStructsFieldsMethodsAndTypes();
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Add the test target before implementation**

In `CMakeLists.txt`, append this target after the existing `module_symbols_tests` block:

```cmake
add_executable(module_interface_emitter_tests
    tests/module_interface_emitter_tests.cpp
    src/ModuleInterfaceEmitter.cpp
    src/TypeUtils.cpp
)
target_include_directories(module_interface_emitter_tests PRIVATE include)
compiler_design_apply_warnings(module_interface_emitter_tests)
compiler_design_apply_sanitizers(module_interface_emitter_tests)
add_test(NAME module_interface_emitter COMMAND module_interface_emitter_tests)
```

Also add `src/ModuleInterfaceEmitter.cpp` to the main `compiler_design` source list immediately after `src/ModuleSymbols.cpp`:

```cmake
    src/ModuleInterfaceEmitter.cpp
```

- [ ] **Step 3: Run the focused test and verify it fails to compile**

Run:

```sh
cmake -S . -B build
cmake --build build --target module_interface_emitter_tests
```

Expected: build fails because `ModuleInterface.hpp`, `ModuleInterfaceEmitter.hpp`, and `src/ModuleInterfaceEmitter.cpp` do not exist yet.

- [ ] **Step 4: Commit the failing test**

```sh
git add CMakeLists.txt tests/module_interface_emitter_tests.cpp
git commit -m "test: add module interface emitter coverage"
```

---

### Task 2: Implement module-interface data structs and emitter

**Files:**
- Create: `include/ModuleInterface.hpp`
- Create: `include/ModuleInterfaceEmitter.hpp`
- Create: `src/ModuleInterfaceEmitter.cpp`
- Test: `tests/module_interface_emitter_tests.cpp`

- [ ] **Step 1: Create the data model header**

Create `include/ModuleInterface.hpp`:

```cpp
#pragma once

#include "TypeUtils.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct ModuleInterfaceValue {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceField {
    std::string name;
    TypeInfo type;
};

struct ModuleInterfaceMethod {
    std::string name;
    std::vector<TypeInfo> parameterTypes;
    TypeInfo returnType;
};

struct ModuleInterfaceStruct {
    std::string name;
    std::vector<ModuleInterfaceField> fields;
    std::vector<ModuleInterfaceMethod> methods;
};

struct ModuleInterface {
    std::size_t moduleId = 0;
    std::string path;
    bool isEntry = false;
    std::vector<ModuleInterfaceValue> values;
    std::vector<ModuleInterfaceStruct> structs;
};
```

- [ ] **Step 2: Create the emitter header**

Create `include/ModuleInterfaceEmitter.hpp`:

```cpp
#pragma once

#include "ModuleInterface.hpp"

#include <iosfwd>
#include <vector>

void writeModuleInterfaceText(std::ostream& out, const std::vector<ModuleInterface>& interfaces);
```

- [ ] **Step 3: Create the emitter implementation**

Create `src/ModuleInterfaceEmitter.cpp`:

```cpp
#include "ModuleInterfaceEmitter.hpp"

#include "TypeUtils.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <vector>

namespace {

template <typename T, typename NameGetter>
std::vector<T> sortedByName(std::vector<T> values, NameGetter name)
{
    std::sort(values.begin(), values.end(), [&](const T& left, const T& right) {
        return name(left) < name(right);
    });
    return values;
}

std::vector<ModuleInterface> sortedModules(std::vector<ModuleInterface> modules)
{
    std::sort(modules.begin(), modules.end(), [](const ModuleInterface& left, const ModuleInterface& right) {
        return left.moduleId < right.moduleId;
    });
    return modules;
}

void writeMethodSignature(std::ostream& out, const ModuleInterfaceMethod& method)
{
    out << method.name << '(';
    for (std::size_t i = 0; i < method.parameterTypes.size(); ++i) {
        if (i != 0) {
            out << ", ";
        }
        out << typeInfoName(method.parameterTypes[i]);
    }
    out << "): " << typeInfoName(method.returnType);
}

} // namespace

void writeModuleInterfaceText(std::ostream& out, const std::vector<ModuleInterface>& interfaces)
{
    std::vector<ModuleInterface> modules = sortedModules(interfaces);
    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        if (moduleIndex != 0) {
            out << '\n';
        }

        const ModuleInterface& module = modules[moduleIndex];
        out << "module " << module.moduleId;
        if (module.isEntry) {
            out << " entry";
        }
        out << " \"" << module.path << "\"\n";

        std::vector<ModuleInterfaceValue> values = sortedByName(module.values, [](const ModuleInterfaceValue& value) {
            return value.name;
        });
        for (const ModuleInterfaceValue& value : values) {
            out << "  export value " << value.name << ": " << typeInfoName(value.type) << "\n";
        }

        std::vector<ModuleInterfaceStruct> structs = sortedByName(module.structs, [](const ModuleInterfaceStruct& structInfo) {
            return structInfo.name;
        });
        for (ModuleInterfaceStruct structInfo : structs) {
            out << "  export struct " << structInfo.name << "\n";
            for (const ModuleInterfaceField& field : structInfo.fields) {
                out << "    field " << field.name << ": " << typeInfoName(field.type) << "\n";
            }

            std::vector<ModuleInterfaceMethod> methods = sortedByName(structInfo.methods, [](const ModuleInterfaceMethod& method) {
                return method.name;
            });
            for (const ModuleInterfaceMethod& method : methods) {
                out << "    method ";
                writeMethodSignature(out, method);
                out << "\n";
            }
        }
    }
}
```

- [ ] **Step 4: Run focused tests**

Run:

```sh
cmake --build build --target module_interface_emitter_tests
ctest --test-dir build --output-on-failure -R module_interface_emitter
```

Expected: build succeeds and CTest reports `100% tests passed` for `module_interface_emitter`.

- [ ] **Step 5: Commit the data model and emitter**

```sh
git add include/ModuleInterface.hpp include/ModuleInterfaceEmitter.hpp src/ModuleInterfaceEmitter.cpp
git commit -m "feat: add module interface emitter"
```

---

### Task 3: Snapshot module interfaces from the type checker

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`
- Test indirectly first through the CLI fixture in Task 4; focused C++ emitter tests remain green.

- [ ] **Step 1: Update `TypeChecker.hpp` public/private API**

Add this include near the other includes:

```cpp
#include "ModuleInterface.hpp"
```

Change the public section from:

```cpp
class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);
```

to:

```cpp
class TypeChecker {
public:
    const ResolvedNames& check(const Program& program);
    const std::vector<ModuleInterface>& moduleInterfaces() const;
```

Add this private declaration near `findModule`:

```cpp
    void buildModuleInterfaces(const Program& program);
```

Add this member near `ModuleSymbols moduleSymbols_;`:

```cpp
    std::vector<ModuleInterface> moduleInterfaces_;
```

- [ ] **Step 2: Clear/build snapshots in `TypeChecker::check`**

In `src/TypeChecker.cpp`, add this method after `TypeChecker::check`:

```cpp
const std::vector<ModuleInterface>& TypeChecker::moduleInterfaces() const
{
    return moduleInterfaces_;
}
```

Inside `TypeChecker::check`, after `moduleSymbols_.clear();`, add:

```cpp
    moduleInterfaces_.clear();
```

At the end of `TypeChecker::check`, replace:

```cpp
    currentProgram_ = nullptr;
    return resolvedNames_;
```

with:

```cpp
    buildModuleInterfaces(program);
    currentProgram_ = nullptr;
    return resolvedNames_;
```

- [ ] **Step 3: Implement snapshot construction**

Add this helper after `findModule` in `src/TypeChecker.cpp`:

```cpp
void TypeChecker::buildModuleInterfaces(const Program& program)
{
    moduleInterfaces_.clear();

    for (const auto& statement : program.statements) {
        const auto* module = dynamic_cast<const ModuleStmt*>(statement.get());
        if (!module) {
            continue;
        }

        ModuleInterface interfaceInfo;
        interfaceInfo.moduleId = module->moduleId;
        interfaceInfo.path = module->path;
        interfaceInfo.isEntry = module->isEntry;

        if (const ModuleValueExports* exports = moduleSymbols_.valueExports(module->moduleId)) {
            for (const auto& entry : *exports) {
                interfaceInfo.values.push_back(ModuleInterfaceValue{entry.first, entry.second.type});
            }
        }

        if (const ModuleStructExports* structExports = moduleSymbols_.structExports(module->moduleId)) {
            for (const auto& entry : *structExports) {
                ModuleInterfaceStruct structInfo;
                structInfo.name = entry.first;
                for (const StructFieldType& field : entry.second.fields) {
                    structInfo.fields.push_back(ModuleInterfaceField{field.name.lexeme, field.type});
                }

                if (const ModuleMethodExports* methodExports = moduleSymbols_.methodExports(module->moduleId)) {
                    const auto methodsForStruct = methodExports->find(entry.first);
                    if (methodsForStruct != methodExports->end()) {
                        for (const auto& methodEntry : methodsForStruct->second) {
                            structInfo.methods.push_back(ModuleInterfaceMethod{
                                methodEntry.first,
                                methodEntry.second.parameterTypes,
                                methodEntry.second.returnType});
                        }
                    }
                }

                interfaceInfo.structs.push_back(std::move(structInfo));
            }
        }

        moduleInterfaces_.push_back(std::move(interfaceInfo));
    }
}
```

- [ ] **Step 4: Run focused tests and build main compiler**

Run:

```sh
cmake --build build --target compiler_design module_interface_emitter_tests
ctest --test-dir build --output-on-failure -R module_interface_emitter
```

Expected: compiler and emitter tests build; `module_interface_emitter` passes.

- [ ] **Step 5: Commit the type-checker snapshot API**

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp
git commit -m "feat: snapshot module interfaces after type checking"
```

---

### Task 4: Add `--module-interface` CLI mode and golden runner support

**Files:**
- Modify: `src/main.cpp`
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`
- Create: `tests/golden/module_interface_exports/input.cd`
- Create: `tests/golden/module_interface_exports/geometry.cd`
- Create: `tests/golden/module_interface_exports/math.cd`
- Create: `tests/golden/module_interface_exports/api.cd`
- Create after update: `tests/golden/module_interface_exports/module-interface.out`

- [ ] **Step 1: Add the golden fixture source files**

Create `tests/golden/module_interface_exports/input.cd`:

```cd
import "./api.cd";
import "./geometry.cd" as geo;

let answer: number = 42;
let privateValue = "hidden";
export answer;
export Point, makePoint from "./geometry.cd";
```

Create `tests/golden/module_interface_exports/geometry.cd`:

```cd
struct Point {
  x: number,
  y: number,
}

impl Point {
  fun length(): number {
    return sqrt((this.x * this.x) + (this.y * this.y));
  }

  fun translate(dx: number, dy: number): Point {
    return Point { x: this.x + dx, y: this.y + dy };
  }
}

fun makePoint(x: number, y: number): Point {
  return Point { x: x, y: y };
}

let origin: Point = Point { x: 0, y: 0 };
let privateGeometry = 99;
export Point, makePoint, origin;
```

Create `tests/golden/module_interface_exports/math.cd`:

```cd
fun twice(value: number): number {
  return value * 2;
}

export twice;
```

Create `tests/golden/module_interface_exports/api.cd`:

```cd
export twice from "./math.cd";
```

- [ ] **Step 2: Add CLI flag parsing and output**

In `src/main.cpp`, add the emitter include near the other includes:

```cpp
#include "ModuleInterfaceEmitter.hpp"
```

Update usage line 1 from:

```cpp
std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [-I dir] [--import-path dir] [file ...]\n"
```

to:

```cpp
std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [--module-interface] [-I dir] [--import-path dir] [file ...]\n"
```

Add a flag near `bool showBytecode = false;`:

```cpp
    bool showModuleInterface = false;
```

Add parsing after the `--bytecode` branch:

```cpp
        } else if (arg == "--module-interface") {
            showModuleInterface = true;
```

Update the `emitBytecodePath` validation from:

```cpp
        if (inputPaths.empty() || showTokens || showIr || showBytecode) {
```

to:

```cpp
        if (inputPaths.empty() || showTokens || showIr || showBytecode || showModuleInterface) {
```

Replace the default AST condition:

```cpp
        if (!emitBytecodePath && !showIr && !showBytecode) {
            program.print(std::cout);
        }
```

with:

```cpp
        if (!emitBytecodePath && !showIr && !showBytecode && !showModuleInterface) {
            program.print(std::cout);
        }

        if (showModuleInterface) {
            writeModuleInterfaceText(std::cout, typeChecker.moduleInterfaces());
        }
```

Leave `showTokens` behavior unchanged: if a user combines `--tokens --module-interface`, tokens print first, then module-interface output after type checking, matching the existing permissive combination style for `--tokens --ir`.

- [ ] **Step 3: Add golden runner mode**

In `tests/run_golden_tests.py`, update `SUCCESS_CHECKS` from:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
)
```

to:

```python
SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
    ("--module-interface", ("--module-interface",), "module-interface.out"),
)
```

- [ ] **Step 4: Update golden runner selftests for the new optional output**

In `tests/run_golden_tests_selftest.py`, replace every hard-coded tuple of compiler success goldens:

```python
("ast.out", "ir.out", "bytecode.out")
```

with:

```python
("ast.out", "ir.out", "bytecode.out", "module-interface.out")
```

In `test_update_rewrites_only_existing_success_outputs_by_default`, add this assertion after the existing `bytecode.out` assertion:

```python
            self.assertFalse((case_dir / "module-interface.out").exists())
```

In `test_update_missing_creates_missing_success_outputs_explicitly`, update the expected result count from:

```python
            self.assertEqual(len(results), 3)
```

to:

```python
            self.assertEqual(len(results), 4)
```

Then add this new selftest after `test_success_case_with_bytecode_expected_file_runs_bytecode_flag`:

```python
    def test_success_case_with_module_interface_expected_file_runs_module_interface_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "module_interface_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "module-interface.out").write_text("module interface output\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--module-interface" not in sys.argv:
                        sys.stderr.write("missing --module-interface\\n")
                        raise SystemExit(1)
                    sys.stdout.write("module interface output\\n")
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "module_interface_case --module-interface")
```

- [ ] **Step 5: Build and generate the new golden output**

Run:

```sh
cmake --build build --target compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --case module_interface_exports --update-missing --update
```

Expected: creates `tests/golden/module_interface_exports/module-interface.out`. Inspect it with:

```sh
cat tests/golden/module_interface_exports/module-interface.out
```

Expected properties:

- contains `module 0 entry` for `input.cd`;
- contains `export value answer: number`;
- contains `export struct Point` and methods `length(): number` and `translate(number, number): Point`;
- contains `export value makePoint: fun(number, number): Point`;
- contains re-exported `twice: fun(number): number` on `api.cd`;
- does not contain `privateValue`, `privateGeometry`, or namespace alias `geo`.

If the exact module ids differ from the assumptions, keep the generated order as long as ids are ascending and the semantic properties above hold.

- [ ] **Step 6: Run targeted tests**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case module_interface_exports
python3 tests/run_golden_tests_selftest.py
```

Expected: both commands pass.

- [ ] **Step 7: Commit CLI and golden runner support**

```sh
git add src/main.cpp tests/run_golden_tests.py tests/run_golden_tests_selftest.py tests/golden/module_interface_exports
if git diff --cached --quiet; then echo "nothing staged" && exit 1; fi
git commit -m "feat: add module interface CLI output"
```

---

### Task 5: Add import-search-path coverage for `--module-interface`

**Files:**
- Modify: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Add a focused CLI test**

Append this test method before the final `if __name__ == "__main__":` block in `tests/cli_multi_source_tests.py`:

```python
    def test_module_interface_uses_import_search_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            app.mkdir()
            stdlib.mkdir()
            (app / "input.cd").write_text('import "math";\nexport value;\n', encoding="utf-8")
            (stdlib / "math.cd").write_text('let value: number = 7;\nexport value;\n', encoding="utf-8")

            completed = self.run_compiler("-I", str(stdlib), "--module-interface", str(app / "input.cd"))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stderr, "")
            self.assertIn(f'module 0 entry "{app / "input.cd"}"\n', completed.stdout)
            self.assertIn('  export value value: number\n', completed.stdout)
            self.assertIn(f'module 1 "{stdlib / "math.cd"}"\n', completed.stdout)
```

- [ ] **Step 2: Run the focused Python test**

Run:

```sh
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: all CLI multi-source tests pass, including the new import search path module-interface test.

- [ ] **Step 3: Commit import search path coverage**

```sh
git add tests/cli_multi_source_tests.py
git commit -m "test: cover module interface import search paths"
```

---

### Task 6: Document the CLI mode

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Find the CLI usage section**

Run:

```sh
grep -n "compiler_design\|--ir\|--bytecode\|--emit-bytecode" README.md | head -n 80
```

Expected: shows the current CLI section or examples.

- [ ] **Step 2: Add `--module-interface` documentation**

Add this concise description near the existing CLI debug-output modes:

```markdown
- `--module-interface` prints the type-checked public API metadata for every loaded module. It includes exported values, named structs, struct fields, and exported struct method signatures. It is a debug/introspection mode only; it does not emit a separate-compilation artifact or run a linker.
```

If README has a usage block, include `--module-interface` in that block next to `--ir` and `--bytecode`.

- [ ] **Step 3: Run documentation-adjacent tests**

Run:

```sh
ctest --test-dir build --output-on-failure -R cmake_config
```

Expected: CTest reports `cmake_config` passes. This does not validate README text directly, but confirms no accidental CMake test regression while docs are staged.

- [ ] **Step 4: Commit docs**

```sh
git add README.md
git commit -m "docs: document module interface output"
```

---

### Task 7: Final verification and cleanup

**Files:**
- Potentially remove generated cache directory: `tests/__pycache__/`

- [ ] **Step 1: Run the full required verification suite**

Run from repository root:

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

Expected: every command before cleanup exits with status 0. `rm -rf tests/__pycache__` removes generated Python cache files if present.

- [ ] **Step 2: Inspect git status**

Run:

```sh
git status --short
```

Expected: clean working tree. If there are intentional uncommitted golden updates or docs changes, inspect them and commit them before finishing. If there are generated files such as `tests/__pycache__/`, remove them.

- [ ] **Step 3: Record final verification in the completion response**

The final response must list the exact verification commands and results, for example:

```text
Verification:
- cmake -S . -B build — passed
- cmake --build build — passed
- ctest --test-dir build --output-on-failure — passed
- python3 tests/run_golden_tests.py ./build/compiler_design — passed
- python3 tests/run_golden_tests_selftest.py — passed
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs — passed
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens — passed
- cargo test --manifest-path vm-rs/Cargo.toml — passed
- rm -rf tests/__pycache__ — completed
```

- [ ] **Step 4: Commit any final cleanup if needed**

If Task 7 changed tracked files, commit them with an accurate message. If it only removed untracked cache files, do not create a commit.
