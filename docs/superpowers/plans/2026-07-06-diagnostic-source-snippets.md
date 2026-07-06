# Diagnostic Source Snippets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Append combined-source lines and caret markers to CLI diagnostics that already have a `SourceLocation`, without changing the existing first diagnostic line.

**Architecture:** Keep `DiagnosticError::what()` as the stable one-line diagnostic. Add `formatDiagnosticWithSource(const DiagnosticError&, const std::string&)` in the diagnostic module, and have `main.cpp` catch `DiagnosticError` before `std::exception` to print the enhanced CLI text using the loaded combined source. Add focused golden/CLI coverage for snippets while preserving existing one-line parse/type fixtures through a targeted golden-runner compatibility rule.

**Tech Stack:** C++17 compiler CLI, existing `DiagnosticError` hierarchy, Python golden/selftest runners, CMake/CTest.

---

## File Structure

- Modify `include/Diagnostic.hpp`: declare `formatDiagnosticWithSource()`.
- Modify `src/Diagnostic.cpp`: implement source-line extraction and caret formatting.
- Modify `src/main.cpp`: keep the combined source visible to catch blocks; catch `DiagnosticError` first and print enhanced diagnostics.
- Modify `tests/run_golden_tests.py`: for parse/type fixtures, allow legacy one-line expected stderr to match actual located diagnostics with appended snippets; exact matching remains for multi-line snippet fixtures and all other categories.
- Modify `tests/run_golden_tests_selftest.py`: add selftests for the parse/type compatibility rule and full multi-line snippet exact matching.
- Modify `tests/golden/parse_errors/call_missing_argument.err`: selected parse fixture with full snippet.
- Modify `tests/golden/type_errors/undefined_variable.err`: selected type fixture with full snippet.
- Modify `tests/cli_multi_source_tests.py`: add focused CLI tests for lex-error snippets and locationless import errors staying one line.
- Modify `README.md`, `AGENTS.md`, `docs/roadmap.md`: document snippet diagnostics and mark Phase 15A implemented.

---

### Task 1: RED tests for diagnostic snippets

**Files:**
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`
- Modify: `tests/golden/parse_errors/call_missing_argument.err`
- Modify: `tests/golden/type_errors/undefined_variable.err`
- Modify: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Add helper for located-diagnostic stderr matching in golden runner**

In `tests/run_golden_tests.py`, add this function after `unified_diff()`:

```python
def located_diagnostic_stderr_matches(expected: str, actual: str) -> bool:
    if actual == expected:
        return True
    expected_lines = expected.splitlines()
    actual_lines = actual.splitlines()
    if len(expected_lines) != 1 or len(actual_lines) < 3:
        return False
    first_line = expected_lines[0]
    if actual_lines[0] != first_line:
        return False
    return actual_lines[1].startswith("  ") and actual_lines[2].startswith("  ") and actual_lines[2].rstrip().endswith("^")
```

In `check_parse_error_case()`, replace:

```python
        if actual_err != expected_err:
            diff = unified_diff(expected_err, actual_err, "expected stderr", "actual stderr")
            results.append(CheckResult(case_name, False, f"FAIL {case_name} stderr mismatch\n\n{diff}"))
```

with:

```python
        if not located_diagnostic_stderr_matches(expected_err, actual_err):
            diff = unified_diff(expected_err, actual_err, "expected stderr", "actual stderr")
            results.append(CheckResult(case_name, False, f"FAIL {case_name} stderr mismatch\n\n{diff}"))
```

Make the same replacement in `check_type_error_case()` only. Do not use this compatibility helper for runtime or import errors.

- [ ] **Step 2: Add golden-runner selftests for snippet matching**

Append these tests before the final `if __name__ == "__main__":` in `tests/run_golden_tests_selftest.py`:

```python
    def test_parse_error_one_line_expected_accepts_actual_source_snippet(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            parse_dir = golden_dir / "parse_errors"
            parse_dir.mkdir(parents=True)
            (parse_dir / "snippet.cd").write_text("print ;\n", encoding="utf-8")
            (parse_dir / "snippet.err").write_text("Parse error at 1:7: expected expression\n", encoding="utf-8")
            (parse_dir / "snippet.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="Parse error at 1:7: expected expression\n  print ;\n        ^\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)

    def test_parse_error_multiline_expected_requires_exact_source_snippet(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            parse_dir = golden_dir / "parse_errors"
            parse_dir.mkdir(parents=True)
            (parse_dir / "snippet.cd").write_text("print ;\n", encoding="utf-8")
            (parse_dir / "snippet.err").write_text(
                "Parse error at 1:7: expected expression\n"
                "  print ;\n"
                "        ^\n",
                encoding="utf-8",
            )
            (parse_dir / "snippet.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="Parse error at 1:7: expected expression\n  print ;\n       ^\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("stderr mismatch", results[0].message)

    def test_import_error_one_line_expected_does_not_accept_extra_snippet(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            import_dir = golden_dir / "import_errors"
            import_dir.mkdir(parents=True)
            (import_dir / "missing.cd").write_text('import "./missing.cd";\n', encoding="utf-8")
            (import_dir / "missing.err").write_text("Import error: failed to open import: missing.cd\n", encoding="utf-8")
            (import_dir / "missing.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="Import error: failed to open import: missing.cd\n  import \"./missing.cd\";\n  ^\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("stderr mismatch", results[0].message)
```

- [ ] **Step 3: Update selected parse/type fixtures to require full snippets**

Replace `tests/golden/parse_errors/call_missing_argument.err` with:

```text
Parse error at 1:14: expected expression
  print add(1, );
               ^
```

Replace `tests/golden/type_errors/undefined_variable.err` with:

```text
Type error at 1:7: undefined variable `missing`
  print missing;
        ^
```

- [ ] **Step 4: Add focused CLI tests for lex snippets and locationless import errors**

In `tests/cli_multi_source_tests.py`, add these methods to `CliMultiSourceTests` before the final `if __name__ == "__main__":`:

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

    def test_import_error_stays_one_line_without_source_snippet(self) -> None:
        completed = self.run_compiler(input_text='import "./lib.cd";\n')

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")
```

If `test_import_from_stdin_is_rejected` already covers the same locationless behavior, keep both tests temporarily during RED/GREEN or replace the older test with this more explicit name during refactor.

- [ ] **Step 5: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R 'golden_runner_selftest|cli_multi_source'
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected before implementation:

- `golden_runner_selftest` passes if the runner changes are correct.
- `cli_multi_source` fails on `test_lex_error_in_stdin_prints_source_snippet` because diagnostics are still one line.
- golden tests fail on `parse_errors/call_missing_argument` and `type_errors/undefined_variable` because those selected fixtures now expect snippets.

- [ ] **Step 6: Commit RED tests**

```bash
git add tests/run_golden_tests.py tests/run_golden_tests_selftest.py tests/golden/parse_errors/call_missing_argument.err tests/golden/type_errors/undefined_variable.err tests/cli_multi_source_tests.py
git commit -m "test: add diagnostic source snippet coverage"
```

---

### Task 2: Implement diagnostic source snippet formatting

**Files:**
- Modify: `include/Diagnostic.hpp`
- Modify: `src/Diagnostic.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Declare CLI snippet formatter**

In `include/Diagnostic.hpp`, add this declaration after `formatDiagnostic(...)`:

```cpp
std::string formatDiagnosticWithSource(const DiagnosticError& error, const std::string& source);
```

- [ ] **Step 2: Implement line extraction and caret formatting**

In `src/Diagnostic.cpp`, add `#include <sstream>` after the existing include block:

```cpp
#include <sstream>
```

Add this helper namespace before `diagnosticKindName()`:

```cpp
namespace {

std::optional<std::string> sourceLineAt(const std::string& source, int requestedLine)
{
    if (requestedLine <= 0) {
        return std::nullopt;
    }

    int currentLine = 1;
    std::size_t lineStart = 0;
    for (std::size_t index = 0; index <= source.size(); ++index) {
        if (index == source.size() || source[index] == '\n') {
            if (currentLine == requestedLine) {
                return source.substr(lineStart, index - lineStart);
            }
            ++currentLine;
            lineStart = index + 1;
        }
    }

    return std::nullopt;
}

std::size_t caretColumn(int column)
{
    if (column <= 1) {
        return 0;
    }
    return static_cast<std::size_t>(column - 1);
}

} // namespace
```

Add this function after `formatDiagnostic(...)`:

```cpp
std::string formatDiagnosticWithSource(const DiagnosticError& error, const std::string& source)
{
    std::ostringstream formatted;
    formatted << error.what();

    const std::optional<SourceLocation>& location = error.location();
    if (!location) {
        return formatted.str();
    }

    std::optional<std::string> line = sourceLineAt(source, location->line);
    if (!line) {
        return formatted.str();
    }

    formatted << '\n'
              << "  " << *line << '\n'
              << "  " << std::string(caretColumn(location->column), ' ') << '^';
    return formatted.str();
}
```

- [ ] **Step 3: Print enhanced diagnostics from the CLI**

In `src/main.cpp`, change the source declaration and catch blocks.

Before the `try`, add:

```cpp
    std::string source;
```

Replace the current source-loading declaration inside the `try`:

```cpp
        const std::string source = inputPaths.empty()
            ? sourceManager.loadStdin(std::cin)
            : sourceManager.loadFiles(inputPaths);
```

with:

```cpp
        source = inputPaths.empty()
            ? sourceManager.loadStdin(std::cin)
            : sourceManager.loadFiles(inputPaths);
```

Replace the final catch block:

```cpp
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
```

with:

```cpp
    } catch (const DiagnosticError& error) {
        std::cerr << formatDiagnosticWithSource(error, source) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
```

- [ ] **Step 4: Verify GREEN for focused tests**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R 'golden_runner_selftest|cli_multi_source'
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected:

- `golden_runner_selftest` passes.
- `cli_multi_source` passes, including lex snippet and import one-line tests.
- golden tests pass. Selected parse/type fixtures match exact multi-line snippets; other parse/type one-line fixtures pass via compatibility helper.

- [ ] **Step 5: Commit implementation**

```bash
git add include/Diagnostic.hpp src/Diagnostic.cpp src/main.cpp
git commit -m "feat: print diagnostic source snippets"
```

---

### Task 3: Documentation and roadmap updates

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update README diagnostics section**

In `README.md`, replace the diagnostics example block:

```markdown
```text
Parse error at 1:15: expected expression
Type error at 1:7: undefined variable `missing`
```
```

with:

````markdown
```text
Parse error at 1:14: expected expression
  print add(1, );
               ^
Type error at 1:7: undefined variable `missing`
  print missing;
        ^
```
````

Replace:

```markdown
Runtime diagnostics currently do not include source locations.
```

with:

```markdown
Located front-end diagnostics include the combined-source line and a caret. Locationless diagnostics, including import, compile, and runtime errors, remain one-line messages.
```

- [ ] **Step 2: Update AGENTS diagnostic convention**

In `AGENTS.md`, replace the diagnostic shape block:

```text
<Kind> error at <line>:<column>: <message>
<Kind> error: <message>
```

with:

```text
<Kind> error at <line>:<column>: <message>
  <source line>
  <caret>
<Kind> error: <message>
```

Replace the sentence beginning `Use locations for lexer, parser, and type errors` with:

```markdown
Use locations for lexer, parser, and type errors when a source token/location is available. The CLI appends a combined-source line and caret for located diagnostics. Compile, import, and runtime diagnostics are currently locationless. After intentional diagnostic format changes, refresh and review parse/type/runtime/import error goldens. Lexer errors do not yet have a dedicated golden fixture category.
```

Add this sentence to the Golden Test Conventions section after the parse/type/import error fixture descriptions:

```markdown
Parse/type error `.err` files may either contain the first diagnostic line only or the full snippet form. Use the full snippet form for fixtures that intentionally cover caret placement.
```

- [ ] **Step 3: Update roadmap Phase 15**

In `docs/roadmap.md`, change the Phase 15 status text by inserting this line after the `Goal:` paragraph:

```markdown
Status: in progress. Phase 15A is implemented: located front-end diagnostics print the combined-source line and a caret while keeping the existing first diagnostic line stable. File-aware diagnostic remapping remains future work.
```

In the suggested features list, change:

```markdown
- Source snippets and carets for front-end diagnostics.
```

into:

```markdown
- Source snippets and carets for front-end diagnostics. Implemented for combined-source locations.
```

- [ ] **Step 4: Verify docs and tests**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: both pass.

- [ ] **Step 5: Commit docs**

```bash
git add README.md AGENTS.md docs/roadmap.md
git commit -m "docs: document diagnostic source snippets"
```

---

### Task 4: Full verification and push

**Files:**
- No planned source edits.
- Remove generated `tests/__pycache__/` if present.

- [ ] **Step 1: Check status before full verification**

Run:

```bash
git status --short --branch
```

Expected: branch is `master`; only intentional committed work is ahead of `origin/master`, or worktree contains only files from completed tasks before final docs commit.

- [ ] **Step 2: Run full verification suite**

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
```

Expected: every command before `rm -rf` exits 0.

- [ ] **Step 3: Inspect final state**

Run:

```bash
git status --short --branch
git log --oneline --decorate -6
```

Expected: worktree is clean and `master` is ahead of `origin/master` by the Phase 15A spec/plan/test/implementation/docs commits.

- [ ] **Step 4: Push to master**

Run:

```bash
git push origin master
```

Expected: push succeeds.

- [ ] **Step 5: Final report**

Report these items to the user:

```text
完成 Phase 15A diagnostic source snippets 并已 push。

实现：
- 带 SourceLocation 的 Lex/Parse/Type diagnostics 追加 combined-source line + caret。
- `DiagnosticError::what()` 保持单行格式；CLI catch 层负责追加 snippet。
- Import/Compile/Runtime/locationless diagnostics 保持一行。
- Golden runner 支持旧一行 parse/type expected，同时选定 fixtures 严格验证完整 snippet。

验证：
- cmake -S . -B build ✅
- cmake --build build ✅
- ctest --test-dir build --output-on-failure ✅
- python3 tests/run_golden_tests.py ./build/compiler_design ✅
- python3 tests/run_golden_tests_selftest.py ✅
- python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs ✅
- python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens ✅
- cargo test --manifest-path vm-rs/Cargo.toml ✅
- git push origin master ✅
```

---

## Self-Review

**Spec coverage:** The plan implements combined-source snippets for located Lex/Parse/Type diagnostics, preserves first-line formatting, leaves locationless diagnostics unchanged, avoids file-aware remapping, covers EOF/caret behavior through formatter rules, adds focused parse/type/lex/import tests, and updates README/AGENTS/roadmap.

**Test strategy note:** The spec asks to avoid refreshing all parse/type error goldens. Because CLI behavior changes globally for located diagnostics, the plan adds a targeted golden-runner compatibility rule: old one-line parse/type `.err` files continue to validate the first line while selected multi-line fixtures validate exact snippet placement. Runtime and import error stderr matching remains exact.

**Placeholder scan:** No placeholder markers, deferred-implementation notes, or unstated test-writing steps remain.

**Type consistency:** `formatDiagnosticWithSource(const DiagnosticError&, const std::string&)`, `located_diagnostic_stderr_matches`, and the main catch-block changes use consistent names and signatures across tasks.
