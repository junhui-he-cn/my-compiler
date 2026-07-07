# File-Aware Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make lex, parse, and type diagnostics from file-backed programs report the original file path, file-local line/column, and file-local source snippet.

**Architecture:** Add a file-context diagnostic wrapper in `Diagnostic` and format it in `main.cpp` before ordinary diagnostics. Preserve local token line/column by parsing file-backed programs as `SourceUnit`s when needed, and attach path/source context at lexer/parser/module type-check boundaries. Keep stdin and locationless import/runtime/compile diagnostics unchanged.

**Tech Stack:** C++17 compiler front end, CMake, Python `unittest` CLI tests, Python golden tests, Rust VM parity tests.

---

## File Structure

- `include/Diagnostic.hpp`, `src/Diagnostic.cpp`: define `DiagnosticSourceContext`, `FileDiagnosticError`, and file-aware formatting helpers.
- `include/Ast.hpp`, `src/Ast.cpp`: extend `ModuleStmt` with source text so type checking can wrap module-local diagnostics.
- `include/ModuleProgram.hpp`, `src/ModuleProgram.cpp`: wrap per-unit lex/parse diagnostics with file context and pass source text into `ModuleStmt`.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: wrap located diagnostics thrown while checking a module with that module's file context; unwrap/export diagnostics preserve current behavior for non-module stdin/combined source.
- `src/main.cpp`: catch `FileDiagnosticError` first; use module-aware per-file parsing for direct multi-file file inputs so diagnostics can be file-aware even without imports.
- `tests/cli_multi_source_tests.py`: add focused file-aware diagnostic tests.
- `tests/golden/`: add small exact multiline diagnostic fixtures for imported parse/type cases.
- `README.md`, `docs/roadmap.md`, `AGENTS.md`: document file-aware diagnostics.

---

### Task 1: Add RED tests for file-aware diagnostics

**Files:**
- Modify: `tests/cli_multi_source_tests.py`
- Create: `tests/golden/parse_errors/imported_file_parse_error.cd`
- Create: `tests/golden/parse_errors/imported_file_parse_error_lib.cd`
- Create: `tests/golden/parse_errors/imported_file_parse_error.err`
- Create: `tests/golden/parse_errors/imported_file_parse_error.exit`
- Create: `tests/golden/type_errors/imported_file_type_error.cd`
- Create: `tests/golden/type_errors/imported_file_type_error_lib.cd`
- Create: `tests/golden/type_errors/imported_file_type_error.err`
- Create: `tests/golden/type_errors/imported_file_type_error.exit`

- [ ] **Step 1: Add CLI tests for file-aware diagnostics**

In `tests/cli_multi_source_tests.py`, add these tests inside `CliMultiSourceTests` after `test_import_path_resolves_relative_to_importing_file`:

```python
    def test_imported_file_parse_error_reports_imported_file_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            (root / "input.cd").write_text('import "./lib.cd";\nprint 1;\n', encoding="utf-8")
            lib.write_text('export let value = ;\n', encoding="utf-8")

            completed = self.run_compiler(str(root / "input.cd"))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Parse error at {lib}:1:20: expected expression\n"
                "  export let value = ;\n"
                "                     ^\n",
            )

    def test_imported_file_type_error_reports_imported_file_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            (root / "input.cd").write_text('import "./lib.cd";\nprint value;\n', encoding="utf-8")
            lib.write_text('export let value = missing;\n', encoding="utf-8")

            completed = self.run_compiler(str(root / "input.cd"))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Type error at {lib}:1:20: undefined variable `missing`\n"
                "  export let value = missing;\n"
                "                     ^\n",
            )

    def test_direct_multi_file_parse_error_reports_own_file_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            first = root / "first.cd"
            second = root / "second.cd"
            first.write_text('let ok = 1;\n', encoding="utf-8")
            second.write_text('print ;\n', encoding="utf-8")

            completed = self.run_compiler(str(first), str(second))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Parse error at {second}:1:7: expected expression\n"
                "  print ;\n"
                "        ^\n",
            )

    def test_direct_multi_file_type_error_reports_own_file_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            first = root / "first.cd"
            second = root / "second.cd"
            first.write_text('let ok = 1;\n', encoding="utf-8")
            second.write_text('print missing;\n', encoding="utf-8")

            completed = self.run_compiler(str(first), str(second))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Type error at {second}:1:7: undefined variable `missing`\n"
                "  print missing;\n"
                "        ^\n",
            )
```

- [ ] **Step 2: Add imported parse golden fixture**

Create `tests/golden/parse_errors/imported_file_parse_error.cd`:

```cd
import "./imported_file_parse_error_lib.cd";
print 1;
```

Create `tests/golden/parse_errors/imported_file_parse_error_lib.cd`:

```cd
export let value = ;
```

Create `tests/golden/parse_errors/imported_file_parse_error.err` with the repository-root absolute path that current import-error fixtures already use. Use this exact command to generate the expected first draft after writing the source files:

```sh
printf 'Parse error at %s/tests/golden/parse_errors/imported_file_parse_error_lib.cd:1:20: expected expression\n  export let value = ;\n                     ^\n' "$(pwd)" > tests/golden/parse_errors/imported_file_parse_error.err
```

Create `tests/golden/parse_errors/imported_file_parse_error.exit`:

```text
1
```

- [ ] **Step 3: Add imported type golden fixture**

Create `tests/golden/type_errors/imported_file_type_error.cd`:

```cd
import "./imported_file_type_error_lib.cd";
print value;
```

Create `tests/golden/type_errors/imported_file_type_error_lib.cd`:

```cd
export let value = missing;
```

Create `tests/golden/type_errors/imported_file_type_error.err` with this command:

```sh
printf 'Type error at %s/tests/golden/type_errors/imported_file_type_error_lib.cd:1:20: undefined variable `missing`\n  export let value = missing;\n                     ^\n' "$(pwd)" > tests/golden/type_errors/imported_file_type_error.err
```

Create `tests/golden/type_errors/imported_file_type_error.exit`:

```text
1
```

- [ ] **Step 4: Verify RED**

Run:

```sh
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: the new tests fail because diagnostics still omit file paths or point at combined-source snippets.

- [ ] **Step 5: Commit RED tests**

```sh
git add tests/cli_multi_source_tests.py \
  tests/golden/parse_errors/imported_file_parse_error.* \
  tests/golden/type_errors/imported_file_type_error.*
git commit -m "test: add file-aware diagnostic expectations"
```

---

### Task 2: Add file-context diagnostic wrapper and formatter

**Files:**
- Modify: `include/Diagnostic.hpp`
- Modify: `src/Diagnostic.cpp`

- [ ] **Step 1: Add declarations**

In `include/Diagnostic.hpp`, after `SourceLocation`, add:

```cpp
struct DiagnosticSourceContext {
    std::string path;
    std::string source;
    bool isStdin = false;
};
```

After `DiagnosticError`, add:

```cpp
class FileDiagnosticError final : public DiagnosticError {
public:
    FileDiagnosticError(const DiagnosticError& inner, DiagnosticSourceContext context);

    const DiagnosticSourceContext& sourceContext() const;

private:
    DiagnosticSourceContext context_;
};
```

Add formatter declaration near `formatDiagnosticWithSource`:

```cpp
std::string formatDiagnosticWithSourceContext(const FileDiagnosticError& error);
```

- [ ] **Step 2: Add formatter helpers**

In `src/Diagnostic.cpp`, add this helper in the anonymous namespace:

```cpp
std::string formatFileDiagnosticFirstLine(const FileDiagnosticError& error)
{
    const auto& location = error.location();
    if (!location || error.sourceContext().isStdin || error.sourceContext().path.empty()) {
        return error.what();
    }

    return diagnosticKindName(error.kind()) + " error at "
        + error.sourceContext().path + ":"
        + std::to_string(location->line) + ":"
        + std::to_string(location->column) + ": "
        + error.message();
}
```

- [ ] **Step 3: Implement wrapper constructor and accessor**

In `src/Diagnostic.cpp`, after `DiagnosticError` constructors, add:

```cpp
FileDiagnosticError::FileDiagnosticError(const DiagnosticError& inner, DiagnosticSourceContext context)
    : DiagnosticError(inner.kind(), inner.location().value_or(SourceLocation{}), inner.message())
    , context_(std::move(context))
{
}

const DiagnosticSourceContext& FileDiagnosticError::sourceContext() const
{
    return context_;
}
```

- [ ] **Step 4: Implement file-aware formatter**

In `src/Diagnostic.cpp`, add:

```cpp
std::string formatDiagnosticWithSourceContext(const FileDiagnosticError& error)
{
    if (!error.location()) {
        return error.what();
    }
    if (error.sourceContext().isStdin || error.sourceContext().path.empty()) {
        return formatDiagnosticWithSource(error, error.sourceContext().source);
    }

    std::ostringstream formatted;
    formatted << formatFileDiagnosticFirstLine(error);

    std::optional<std::string> line = sourceLineAt(error.sourceContext().source, error.location()->line);
    if (!line) {
        return formatted.str();
    }

    formatted << '\n'
              << "  " << *line << '\n'
              << "  " << std::string(caretColumn(error.location()->column), ' ') << '^';
    return formatted.str();
}
```

- [ ] **Step 5: Build**

Run:

```sh
cmake --build build
```

Expected: build succeeds.

- [ ] **Step 6: Commit diagnostic wrapper**

```sh
git add include/Diagnostic.hpp src/Diagnostic.cpp
git commit -m "feat: add file diagnostic wrapper"
```

---

### Task 3: Preserve source context in ModuleStmt and module parsing

**Files:**
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `src/ModuleProgram.cpp`

- [ ] **Step 1: Extend ModuleStmt**

In `include/Ast.hpp`, replace the `ModuleStmt` constructor declaration with:

```cpp
    ModuleStmt(std::size_t moduleId, std::string path, std::string source, std::vector<StmtPtr> statements, bool isEntry);
```

Add a field after `path`:

```cpp
    std::string source;
```

- [ ] **Step 2: Update ModuleStmt constructor**

In `src/Ast.cpp`, replace the constructor implementation with:

```cpp
ModuleStmt::ModuleStmt(std::size_t moduleId, std::string path, std::string source, std::vector<StmtPtr> statements, bool isEntry)
    : moduleId(moduleId)
    , path(std::move(path))
    , source(std::move(source))
    , statements(std::move(statements))
    , isEntry(isEntry)
{
}
```

Keep `ModuleStmt::print()` unchanged.

- [ ] **Step 3: Wrap lex/parse diagnostics in ModuleProgram**

In `src/ModuleProgram.cpp`, replace `parseUnit()` with:

```cpp
std::vector<StmtPtr> parseUnit(const SourceUnit& unit)
{
    try {
        Lexer lexer(unit.source);
        Parser parser(lexer.scanTokens());
        Program parsed = parser.parse();
        return std::move(parsed.statements);
    } catch (const FileDiagnosticError&) {
        throw;
    } catch (const DiagnosticError& error) {
        if (error.location()) {
            throw FileDiagnosticError(error, DiagnosticSourceContext{unit.path, unit.source, false});
        }
        throw;
    }
}
```

- [ ] **Step 4: Pass source text to ModuleStmt**

In `buildModuleProgram()`, update the constructor call:

```cpp
        program.statements.push_back(std::make_unique<ModuleStmt>(
            unit.id,
            unit.path,
            unit.source,
            std::move(statements),
            unit.isEntry));
```

- [ ] **Step 5: Build and run parse RED subset**

Run:

```sh
cmake --build build
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: imported parse diagnostics now have file paths; imported type and direct multi-file tests may still fail.

- [ ] **Step 6: Commit module parse context**

```sh
git add include/Ast.hpp src/Ast.cpp src/ModuleProgram.cpp
git commit -m "feat: attach source context to modules"
```

---

### Task 4: Wrap module type-check diagnostics

**Files:**
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Add wrapping helper**

In `src/TypeChecker.cpp`, add this helper in the anonymous namespace near other helpers:

```cpp
void rethrowWithModuleContext(const DiagnosticError& error, const ModuleStmt& module)
{
    if (error.location()) {
        throw FileDiagnosticError(error, DiagnosticSourceContext{module.path, module.source, false});
    }
    throw error;
}
```

- [ ] **Step 2: Wrap checkModule body**

In `TypeChecker::checkModule`, replace this loop:

```cpp
    for (const auto& statement : module.statements) {
        checkStatement(*statement);
    }
```

with:

```cpp
    try {
        for (const auto& statement : module.statements) {
            checkStatement(*statement);
        }
    } catch (const FileDiagnosticError&) {
        throw;
    } catch (const DiagnosticError& error) {
        rethrowWithModuleContext(error, module);
    }
```

- [ ] **Step 3: Build and run imported type tests**

Run:

```sh
cmake --build build
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: imported parse and type diagnostics now have file paths. Direct multi-file file-aware tests may still fail.

- [ ] **Step 4: Commit module type context**

```sh
git add src/TypeChecker.cpp
git commit -m "feat: report module type diagnostics with file context"
```

---

### Task 5: Make direct multi-file diagnostics file-aware

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add helper to detect file-aware direct parsing need**

In `src/main.cpp`, add this helper after `containsImportToken()`:

```cpp
bool shouldUseFileUnitProgram(const SourceLoadResult& loadResult)
{
    return containsImportToken(loadResult) || loadResult.units.size() > 1;
}
```

- [ ] **Step 2: Use module program for direct multi-file inputs**

In `main.cpp`, replace:

```cpp
            if (containsImportToken(loadResult)) {
                program = buildModuleProgram(loadResult);
            } else {
```

with:

```cpp
            if (shouldUseFileUnitProgram(loadResult)) {
                program = buildModuleProgram(loadResult);
            } else {
```

This keeps single file without imports on the existing path, while direct multi-file inputs get per-file parse/type context.

- [ ] **Step 3: Catch FileDiagnosticError first**

In `main.cpp`, before the existing `catch (const DiagnosticError& error)`, add:

```cpp
    } catch (const FileDiagnosticError& error) {
        std::cerr << formatDiagnosticWithSourceContext(error) << '\n';
        return 1;
```

- [ ] **Step 4: Build and run CLI tests**

Run:

```sh
cmake --build build
python3 tests/cli_multi_source_tests.py ./build/compiler_design
```

Expected: all CLI multi-source tests pass.

- [ ] **Step 5: Run golden tests**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: golden tests pass or only the two new exact `.err` files need path/caret adjustment. If they mismatch due to absolute path differences, update only those two files with the exact current output after confirming it points at the imported lib file.

- [ ] **Step 6: Commit direct multi-file file-aware diagnostics**

```sh
git add src/main.cpp tests/golden/parse_errors/imported_file_parse_error.err tests/golden/type_errors/imported_file_type_error.err
git commit -m "feat: report file-aware CLI diagnostics"
```

---

### Task 6: Preserve stdin and locationless diagnostic behavior

**Files:**
- Modify: `tests/cli_multi_source_tests.py`
- Modify: `tests/run_golden_tests_selftest.py` only if golden compatibility needs adjustment.

- [ ] **Step 1: Add explicit stdin regression test if missing**

Confirm `tests/cli_multi_source_tests.py` still contains `test_lex_error_in_stdin_prints_source_snippet`. If not, add:

```python
    def test_lex_error_in_stdin_prints_source_snippet(self) -> None:
        completed = self.run_compiler(input_text="print @;\n")

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(
            completed.stderr,
            "Lex error at 1:7: unexpected character `@`\n"
            "  print @;\n"
            "        ^\n",
        )
```

- [ ] **Step 2: Add explicit import-error regression if missing**

Confirm `tests/cli_multi_source_tests.py` still contains `test_import_error_stays_one_line_without_source_snippet`. If not, add:

```python
    def test_import_error_stays_one_line_without_source_snippet(self) -> None:
        completed = self.run_compiler(input_text='import "./lib.cd";\n')

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")
```

- [ ] **Step 3: Run focused tests**

Run:

```sh
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
```

Expected: both pass.

- [ ] **Step 4: Commit regressions only if files changed**

If Step 1 or 2 modified tests, commit:

```sh
git add tests/cli_multi_source_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: preserve pathless diagnostics"
```

If no files changed, skip this commit.

---

### Task 7: Update documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update README diagnostics section**

In `README.md`, replace the sentence that currently says located diagnostics use combined-source line/caret with:

```md
Located lexer, parser, and type diagnostics include a source line and caret. For file inputs, diagnostics report the original file path plus file-local line and column. For stdin, diagnostics remain pathless. Locationless diagnostics, including import loading, compile, and runtime errors, remain one-line messages.
```

- [ ] **Step 2: Update roadmap Phase 15 status**

In `docs/roadmap.md`, update Phase 15 status to add:

```md
Phase 15C is implemented: file-backed lexer, parser, and type diagnostics report the source file path with file-local snippets, while stdin remains pathless and locationless diagnostics remain one-line.
```

Also remove `File-aware diagnostic remapping remains future work.` from Phase 15 status if present.

- [ ] **Step 3: Update AGENTS diagnostic convention**

In `AGENTS.md`, update the diagnostic section with:

```md
For file-backed lexer, parser, and type diagnostics, the first line may include a file path: `<Kind> error at <path>:<line>:<column>: <message>`. For stdin diagnostics, the first line remains `<Kind> error at <line>:<column>: <message>`. The CLI appends the relevant source line and caret for located diagnostics. Compile, import loading, and runtime diagnostics are currently locationless unless explicitly changed by a future slice.
```

Also update the current semantics bullet that says diagnostics report combined-source line/column so it says file inputs are file-aware and stdin is pathless.

- [ ] **Step 4: Commit docs**

```sh
git add README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document file-aware diagnostics"
```

---

### Task 8: Full verification and cleanup

**Files:**
- Generated expected files only if tests reveal intentional changes.

- [ ] **Step 1: Configure and build**

Run:

```sh
cmake -S . -B build
cmake --build build
```

Expected: build completes with no compiler errors.

- [ ] **Step 2: Run CTest**

Run:

```sh
ctest --test-dir build --output-on-failure
```

Expected: all CTest tests pass.

- [ ] **Step 3: Run full golden suite**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: all golden tests pass.

- [ ] **Step 4: Run golden runner selftests**

Run:

```sh
python3 tests/run_golden_tests_selftest.py
```

Expected: selftests pass.

- [ ] **Step 5: Run bytecode artifact tests**

Run:

```sh
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected: artifact tests pass.

- [ ] **Step 6: Run Rust VM golden parity**

Run:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
```

Expected: Rust VM tests pass with zero failures.

- [ ] **Step 7: Run Rust unit tests**

Run:

```sh
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: Rust tests pass.

- [ ] **Step 8: Remove Python cache**

Run:

```sh
rm -rf tests/__pycache__
```

Expected: no output.

- [ ] **Step 9: Check working tree**

Run:

```sh
git status --short
```

Expected: only intentional files modified. If any expected diagnostics changed, inspect them before committing.

- [ ] **Step 10: Final commit if needed**

If Task 8 produced intentional updates, commit them:

```sh
git add <intentional-files>
git commit -m "test: refresh file-aware diagnostics"
```

---

## Self-Review Notes

- Spec coverage: The plan covers file-backed lex/parse/type diagnostics, stdin pathless behavior, locationless import/runtime/compile behavior, module/import units, direct multi-file inputs, tests, docs, and full verification.
- Scope control: The plan does not add runtime stack traces, LSP diagnostics, token-wide file ids, or Rust VM diagnostic changes.
- Type consistency: `DiagnosticSourceContext`, `FileDiagnosticError`, `formatDiagnosticWithSourceContext`, `ModuleStmt::source`, and `shouldUseFileUnitProgram` are introduced before use.
