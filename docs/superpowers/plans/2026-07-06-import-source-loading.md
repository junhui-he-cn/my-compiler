# Import Source Loading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Phase 14B minimal source-loading `import "path";` so file inputs can load dependency source files relative to the importing file.

**Architecture:** Extend `SourceManager` into the only import-expansion layer: it reads CLI entry files, recursively expands valid top-level import directives into source text, suppresses duplicate canonical paths, and reports import diagnostics before lexing/parsing. Add an `import` token and parser-level malformed-import diagnostics for directives that survive expansion. Keep the rest of the compiler pipeline unchanged after `SourceManager` returns combined source.

**Tech Stack:** C++17 `std::filesystem`, existing C++ lexer/parser/diagnostic pipeline, Python golden and CLI integration tests, `.cdbc` bytecode artifact tests, Rust VM parity tests.

---

## File Structure

- Modify `include/Diagnostic.hpp`: add `DiagnosticKind::Import`.
- Modify `src/Diagnostic.cpp`: format `Import error: ...` diagnostics.
- Modify `include/Token.hpp`: add `TokenType::Import`.
- Modify `src/Lexer.cpp`: lex `import` as a keyword and print token name.
- Modify `include/Parser.hpp`: declare `importDeclaration()`.
- Modify `src/Parser.cpp`: parse surviving malformed import declarations and emit stable parse errors.
- Modify `include/SourceManager.hpp`: add private recursive import-expansion helpers and per-load state.
- Modify `src/SourceManager.cpp`: implement recursive file loading, import scanning, duplicate suppression, cycle detection, and stdin rejection.
- Modify `tests/cli_multi_source_tests.py`: add focused CLI tests for stdin import rejection and import edge cases using temp files.
- Modify `tests/run_golden_tests.py`: add `tests/golden/import_errors/*.cd` support.
- Modify `tests/run_golden_tests_selftest.py`: cover import-error discovery/checking.
- Create success fixtures under `tests/golden/import_basic/`, `tests/golden/import_nested/`, and `tests/golden/import_duplicate/`.
- Create parse-error fixtures under `tests/golden/parse_errors/import_missing_path.*` and `tests/golden/parse_errors/import_missing_semicolon.*`.
- Create import-error fixtures under `tests/golden/import_errors/import_missing_file.*` and `tests/golden/import_errors/import_cycle.*` with sidecar files for the cycle.
- Create bytecode/Rust artifact fixture under `tests/bytecode_artifacts/import_basic/`.
- Modify `README.md`: document `import "path";` semantics and limitations.
- Modify `docs/language-grammar.ebnf`: add `importDecl` to declarations.
- Modify `docs/roadmap.md`: mark Phase 14B source-loading imports complete.
- Modify `AGENTS.md`: add current import semantics/limitations to project memory.

---

### Task 1: RED tests for source-loading imports

**Files:**
- Modify: `tests/cli_multi_source_tests.py`
- Modify: `tests/run_golden_tests.py`
- Modify: `tests/run_golden_tests_selftest.py`
- Create: `tests/golden/import_basic/input.cd`
- Create: `tests/golden/import_basic/lib.cd`
- Create: `tests/golden/import_basic/run.out`
- Create: `tests/golden/import_nested/input.cd`
- Create: `tests/golden/import_nested/lib.cd`
- Create: `tests/golden/import_nested/inner.cd`
- Create: `tests/golden/import_nested/run.out`
- Create: `tests/golden/import_duplicate/input.cd`
- Create: `tests/golden/import_duplicate/lib.cd`
- Create: `tests/golden/import_duplicate/run.out`
- Create: `tests/golden/parse_errors/import_missing_path.cd`
- Create: `tests/golden/parse_errors/import_missing_path.err`
- Create: `tests/golden/parse_errors/import_missing_path.exit`
- Create: `tests/golden/parse_errors/import_missing_semicolon.cd`
- Create: `tests/golden/parse_errors/import_missing_semicolon.err`
- Create: `tests/golden/parse_errors/import_missing_semicolon.exit`
- Create: `tests/golden/import_errors/import_missing_file.cd`
- Create: `tests/golden/import_errors/import_missing_file.err`
- Create: `tests/golden/import_errors/import_missing_file.exit`
- Create: `tests/golden/import_errors/import_cycle.cd`
- Create: `tests/golden/import_errors/import_cycle_a.cd`
- Create: `tests/golden/import_errors/import_cycle_b.cd`
- Create: `tests/golden/import_errors/import_cycle.err`
- Create: `tests/golden/import_errors/import_cycle.exit`
- Create: `tests/bytecode_artifacts/import_basic/input.cd`
- Create: `tests/bytecode_artifacts/import_basic/lib.cd`
- Create: `tests/bytecode_artifacts/import_basic/run.out`

- [ ] **Step 1: Extend golden runner discovery for import errors**

In `tests/run_golden_tests.py`, change the success-case exclusion set inside `discover_success_cases()` to include `"import_errors"`:

```python
and case_dir.name not in {"runtime_errors", "parse_errors", "type_errors", "import_errors"}
```

Add these functions after `discover_type_error_cases()`:

```python
def discover_import_error_cases(golden_dir: Path) -> list[Path]:
    import_dir = golden_dir / "import_errors"
    if not import_dir.is_dir():
        return []
    return sorted(import_dir.glob("*.cd"))
```

Add this helper after `unexpected_type_stdout_result()`:

```python
def unexpected_import_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for import error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )
```

Add this checker after `check_type_error_case()`:

```python
def check_import_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"import_errors/{source.stem} default(ast)"

    completed = run_compiler(compiler, (), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_import_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_import_stdout_result(case_name, completed.stdout))

    if not err_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected stderr file: {err_path}"))
    else:
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

In `run_all()`, add import-error execution after type-error execution:

```python
    for source in discover_import_error_cases(golden_dir):
        results.extend(check_import_error_case(compiler, source, update))
```

- [ ] **Step 2: Add golden-runner selftests for import errors**

Append these tests before the final `if __name__ == "__main__":` in `tests/run_golden_tests_selftest.py`:

```python
    def test_import_error_case_checks_default_mode_stderr_exit_and_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            import_dir = golden_dir / "import_errors"
            import_dir.mkdir(parents=True)
            (import_dir / "missing.cd").write_text('import "./missing_dep.cd";\n', encoding="utf-8")
            (import_dir / "missing.err").write_text("import error\n", encoding="utf-8")
            (import_dir / "missing.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="import error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("import_errors/missing default(ast)", results[0].message)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)

    def test_import_error_input_cd_is_not_discovered_as_success_case(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            import_dir = golden_dir / "import_errors"
            import_dir.mkdir(parents=True)
            (import_dir / "input.cd").write_text('import "./missing_dep.cd";\n', encoding="utf-8")
            (import_dir / "input.err").write_text("import error\n", encoding="utf-8")
            (import_dir / "input.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="import error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "import_errors/input default(ast)")
```

- [ ] **Step 3: Add focused CLI import tests**

Add these methods to `CliMultiSourceTests` in `tests/cli_multi_source_tests.py`:

```python
    def test_import_from_stdin_is_rejected(self) -> None:
        completed = self.run_compiler(input_text='import "./lib.cd";\n')

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")

    def test_import_path_resolves_relative_to_importing_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            nested = root / "nested"
            nested.mkdir()
            (root / "input.cd").write_text('import "./nested/lib.cd";\nprint value;\n', encoding="utf-8")
            (nested / "lib.cd").write_text('import "./inner.cd";\n', encoding="utf-8")
            (nested / "inner.cd").write_text('let value = "relative";\n', encoding="utf-8")

            completed = self.run_compiler("--run", str(root / "input.cd"))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "relative\n")
            self.assertEqual(completed.stderr, "")

    def test_import_text_inside_string_and_comment_is_ignored_by_loader(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "input.cd").write_text(
                '// import "./missing_from_comment.cd";\n'
                'print "import \\\"./missing_from_string.cd\\\";";\n',
                encoding="utf-8",
            )

            completed = self.run_compiler("--run", str(root / "input.cd"))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, 'import "./missing_from_string.cd";\n')
            self.assertEqual(completed.stderr, "")
```

- [ ] **Step 4: Add success golden fixtures**

Create `tests/golden/import_basic/input.cd`:

```cd
import "./lib.cd";
print add(1, 2);
```

Create `tests/golden/import_basic/lib.cd`:

```cd
fun add(a, b) { return a + b; }
```

Create `tests/golden/import_basic/run.out`:

```text
3
```

Create `tests/golden/import_nested/input.cd`:

```cd
import "./lib.cd";
print next();
```

Create `tests/golden/import_nested/lib.cd`:

```cd
import "./inner.cd";
fun next() { return value + 1; }
```

Create `tests/golden/import_nested/inner.cd`:

```cd
let value = 41;
```

Create `tests/golden/import_nested/run.out`:

```text
42
```

Create `tests/golden/import_duplicate/input.cd`:

```cd
import "./lib.cd";
import "./lib.cd";
print value;
```

Create `tests/golden/import_duplicate/lib.cd`:

```cd
let value = 1;
```

Create `tests/golden/import_duplicate/run.out`:

```text
1
```

- [ ] **Step 5: Add malformed import parse-error fixtures**

Create `tests/golden/parse_errors/import_missing_path.cd` with no trailing newline:

```cd
import path;
```

Create `tests/golden/parse_errors/import_missing_path.err`:

```text
Parse error at 1:8: expected import path string
```

Create `tests/golden/parse_errors/import_missing_path.exit`:

```text
1
```

Create `tests/golden/parse_errors/import_missing_semicolon.cd` with no trailing newline:

```cd
import "./lib.cd"
```

Create `tests/golden/parse_errors/import_missing_semicolon.err`:

```text
Parse error at 1:18: expected `;` after import path
```

Create `tests/golden/parse_errors/import_missing_semicolon.exit`:

```text
1
```

- [ ] **Step 6: Add import-error fixtures**

Create `tests/golden/import_errors/import_missing_file.cd`:

```cd
import "./does_not_exist.cd";
```

Create `tests/golden/import_errors/import_missing_file.err`:

```text
Import error: failed to open import: tests/golden/import_errors/does_not_exist.cd
```

Create `tests/golden/import_errors/import_missing_file.exit`:

```text
1
```

Create `tests/golden/import_errors/import_cycle.cd`:

```cd
import "./import_cycle_a.cd";
```

Create `tests/golden/import_errors/import_cycle_a.cd`:

```cd
import "./import_cycle_b.cd";
```

Create `tests/golden/import_errors/import_cycle_b.cd`:

```cd
import "./import_cycle_a.cd";
```

Create `tests/golden/import_errors/import_cycle.err` initially with this relative-shape expectation; refresh it after implementation if the implementation chooses absolute canonical paths:

```text
Import error: import cycle detected: tests/golden/import_errors/import_cycle_a.cd -> tests/golden/import_errors/import_cycle_b.cd -> tests/golden/import_errors/import_cycle_a.cd
```

Create `tests/golden/import_errors/import_cycle.exit`:

```text
1
```

- [ ] **Step 7: Add bytecode/Rust VM import fixture input**

Create `tests/bytecode_artifacts/import_basic/input.cd`:

```cd
import "./lib.cd";
print add(2, 5);
```

Create `tests/bytecode_artifacts/import_basic/lib.cd`:

```cd
fun add(a, b) { return a + b; }
```

Create `tests/bytecode_artifacts/import_basic/run.out`:

```text
7
```

Do not create `expected.cdbc` yet; Task 5 refreshes it after the implementation exists.

- [ ] **Step 8: Verify RED**

Run:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R cli_multi_source
python3 tests/run_golden_tests.py ./build/compiler_design
python3 tests/run_golden_tests_selftest.py
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
```

Expected results before implementation:
- `cli_multi_source` fails on import tests.
- golden tests fail on import fixtures and parse fixtures.
- selftest passes if the runner edits are correct.
- bytecode artifact tests fail because `tests/bytecode_artifacts/import_basic/expected.cdbc` is intentionally missing or import compilation fails.

- [ ] **Step 9: Commit RED tests**

```bash
git add tests/cli_multi_source_tests.py tests/run_golden_tests.py tests/run_golden_tests_selftest.py tests/golden/import_basic tests/golden/import_nested tests/golden/import_duplicate tests/golden/parse_errors/import_missing_path.* tests/golden/parse_errors/import_missing_semicolon.* tests/golden/import_errors tests/bytecode_artifacts/import_basic
git commit -m "test: add import source loading coverage"
```

---

### Task 2: Add import diagnostics, token, and parser malformed-import checks

**Files:**
- Modify: `include/Diagnostic.hpp`
- Modify: `src/Diagnostic.cpp`
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add import diagnostic kind**

In `include/Diagnostic.hpp`, update `DiagnosticKind`:

```cpp
enum class DiagnosticKind {
    Lex,
    Parse,
    Type,
    Compile,
    Runtime,
    Import,
};
```

In `src/Diagnostic.cpp`, add this case to `diagnosticKindName()` after `Runtime`:

```cpp
    case DiagnosticKind::Import:
        return "Import";
```

- [ ] **Step 2: Add import token**

In `include/Token.hpp`, add `Import` after `If`:

```cpp
    If,
    Import,
    Else,
```

In `src/Lexer.cpp`, add keyword mapping:

```cpp
        {"import", TokenType::Import},
```

In `tokenTypeName()`, add:

```cpp
    case TokenType::Import:
        return "Import";
```

- [ ] **Step 3: Add parser declaration for surviving imports**

In `include/Parser.hpp`, add this private method after `letDeclaration()`:

```cpp
    StmtPtr importDeclaration();
```

In `src/Parser.cpp`, update `Parser::declaration()` so `import` is checked before normal statements:

```cpp
StmtPtr Parser::declaration()
{
    if (match(TokenType::Import)) {
        return importDeclaration();
    }
    if (match(TokenType::Struct)) {
        return structDeclaration();
    }
    if (match(TokenType::Fun)) {
        return functionDeclaration();
    }
    if (match(TokenType::Let)) {
        return letDeclaration();
    }
    return statement();
}
```

Add this method after `letDeclaration()`:

```cpp
StmtPtr Parser::importDeclaration()
{
    const Token keyword = previous();
    consume(TokenType::String, "expected import path string");
    consume(TokenType::Semicolon, "expected `;` after import path");
    throw ParseError(keyword, "import declarations must be loaded before parsing");
}
```

This method intentionally has no AST node because valid top-level imports are expanded by `SourceManager`; malformed directives get location-bearing parse errors from `consume()`.

- [ ] **Step 4: Verify malformed-import tests still fail for source loading but parse messages are now correct**

Run:

```bash
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: parse-error fixtures `import_missing_path` and `import_missing_semicolon` pass; import success/error fixtures still fail because `SourceManager` expansion is not implemented.

- [ ] **Step 5: Commit diagnostic/token/parser work**

```bash
git add include/Diagnostic.hpp src/Diagnostic.cpp include/Token.hpp src/Lexer.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: add import diagnostics and parser checks"
```

---

### Task 3: Implement SourceManager recursive import expansion

**Files:**
- Modify: `include/SourceManager.hpp`
- Modify: `src/SourceManager.cpp`

- [ ] **Step 1: Extend SourceManager header**

Replace `include/SourceManager.hpp` with:

```cpp
#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_set>
#include <vector>

struct SourceFile {
    std::string path;
    std::string source;
    std::size_t startLine = 1;
};

class SourceManager {
public:
    std::string loadStdin(std::istream& input);
    std::string loadFiles(const std::vector<std::string>& paths);
    const std::vector<SourceFile>& files() const;

private:
    std::string loadFile(const std::filesystem::path& path, bool isImport);
    std::string expandImports(const std::string& source, const std::filesystem::path& importingFile);
    bool containsTopLevelImportKeyword(const std::string& source) const;

    std::vector<SourceFile> files_;
    std::unordered_set<std::string> loadedFiles_;
    std::vector<std::string> loadingStack_;
};
```

- [ ] **Step 2: Replace SourceManager implementation**

Replace `src/SourceManager.cpp` with:

```cpp
#include "SourceManager.hpp"

#include "Diagnostic.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace {

std::string readAll(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::size_t lineCount(const std::string& source)
{
    std::size_t lines = 1;
    for (char ch : source) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
}

bool isIdentifierStart(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierPart(char ch)
{
    return isIdentifierStart(ch) || std::isdigit(static_cast<unsigned char>(ch));
}

bool isBoundaryBefore(const std::string& source, std::size_t index)
{
    return index == 0 || !isIdentifierPart(source[index - 1]);
}

bool isBoundaryAfter(const std::string& source, std::size_t index)
{
    return index >= source.size() || !isIdentifierPart(source[index]);
}

bool startsWithImportKeyword(const std::string& source, std::size_t index)
{
    constexpr const char* keyword = "import";
    constexpr std::size_t keywordLength = 6;
    return index + keywordLength <= source.size()
        && source.compare(index, keywordLength, keyword) == 0
        && isBoundaryBefore(source, index)
        && isBoundaryAfter(source, index + keywordLength);
}

void skipWhitespace(const std::string& source, std::size_t& index)
{
    while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index]))) {
        ++index;
    }
}

bool parseImportDirective(
    const std::string& source,
    std::size_t start,
    std::string& path,
    std::size_t& afterDirective)
{
    std::size_t index = start + 6;
    skipWhitespace(source, index);
    if (index >= source.size() || source[index] != '"') {
        return false;
    }

    const std::size_t pathStart = index + 1;
    ++index;
    while (index < source.size() && source[index] != '"') {
        ++index;
    }
    if (index >= source.size()) {
        return false;
    }

    path = source.substr(pathStart, index - pathStart);
    ++index;
    skipWhitespace(source, index);
    if (index >= source.size() || source[index] != ';') {
        return false;
    }

    afterDirective = index + 1;
    return true;
}

std::filesystem::path normalizedExistingPath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical.lexically_normal();
    }
    return std::filesystem::absolute(path).lexically_normal();
}

std::string pathString(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string displayCycle(const std::vector<std::string>& stack, const std::string& repeated)
{
    auto found = std::find(stack.begin(), stack.end(), repeated);
    std::ostringstream out;
    if (found == stack.end()) {
        out << repeated << " -> " << repeated;
        return out.str();
    }

    for (auto it = found; it != stack.end(); ++it) {
        if (it != found) {
            out << " -> ";
        }
        out << *it;
    }
    out << " -> " << repeated;
    return out.str();
}

void appendWithNewlineSeparation(std::string& output, const std::string& source)
{
    if (!output.empty() && output.back() != '\n') {
        output.push_back('\n');
    }
    output += source;
    if (!output.empty() && output.back() != '\n') {
        output.push_back('\n');
    }
}

} // namespace

std::string SourceManager::loadStdin(std::istream& input)
{
    files_.clear();
    loadedFiles_.clear();
    loadingStack_.clear();

    std::string source = readAll(input);
    if (containsTopLevelImportKeyword(source)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }
    return source;
}

std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    files_.clear();
    loadedFiles_.clear();
    loadingStack_.clear();

    std::string combined;
    for (const std::string& path : paths) {
        appendWithNewlineSeparation(combined, loadFile(path, false));
    }
    return combined;
}

const std::vector<SourceFile>& SourceManager::files() const
{
    return files_;
}

std::string SourceManager::loadFile(const std::filesystem::path& path, bool isImport)
{
    const std::filesystem::path normalizedPath = normalizedExistingPath(path);
    const std::string canonical = pathString(normalizedPath);
    const std::string display = pathString(path);

    if (std::find(loadingStack_.begin(), loadingStack_.end(), canonical) != loadingStack_.end()) {
        throw DiagnosticError(DiagnosticKind::Import, "import cycle detected: " + displayCycle(loadingStack_, canonical));
    }
    if (loadedFiles_.find(canonical) != loadedFiles_.end()) {
        return "";
    }

    std::ifstream file(path);
    if (!file) {
        if (isImport) {
            throw DiagnosticError(DiagnosticKind::Import, "failed to open import: " + display);
        }
        throw std::runtime_error("failed to open input file: " + display);
    }

    loadingStack_.push_back(canonical);
    std::string source = readAll(file);
    std::string expanded = expandImports(source, normalizedPath);
    loadingStack_.pop_back();

    loadedFiles_.insert(canonical);

    std::size_t startLine = 1;
    if (!files_.empty()) {
        const SourceFile& previous = files_.back();
        startLine = previous.startLine + lineCount(previous.source) - 1;
        if (!previous.source.empty() && previous.source.back() != '\n') {
            ++startLine;
        }
    }
    files_.push_back(SourceFile{display, expanded, startLine});

    return expanded;
}

std::string SourceManager::expandImports(const std::string& source, const std::filesystem::path& importingFile)
{
    std::string output;
    output.reserve(source.size());
    int braceDepth = 0;

    for (std::size_t index = 0; index < source.size();) {
        const char ch = source[index];

        if (ch == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            while (index < source.size() && source[index] != '\n') {
                output.push_back(source[index++]);
            }
            continue;
        }

        if (ch == '"') {
            output.push_back(source[index++]);
            while (index < source.size()) {
                const char stringChar = source[index];
                output.push_back(stringChar);
                ++index;
                if (stringChar == '"') {
                    break;
                }
            }
            continue;
        }

        if (ch == '{') {
            ++braceDepth;
            output.push_back(ch);
            ++index;
            continue;
        }
        if (ch == '}') {
            if (braceDepth > 0) {
                --braceDepth;
            }
            output.push_back(ch);
            ++index;
            continue;
        }

        if (braceDepth == 0 && startsWithImportKeyword(source, index)) {
            std::string importPath;
            std::size_t afterDirective = index;
            if (parseImportDirective(source, index, importPath, afterDirective)) {
                const std::filesystem::path resolved = importingFile.parent_path() / importPath;
                appendWithNewlineSeparation(output, loadFile(resolved, true));
                index = afterDirective;
                continue;
            }
        }

        output.push_back(ch);
        ++index;
    }

    return output;
}

bool SourceManager::containsTopLevelImportKeyword(const std::string& source) const
{
    int braceDepth = 0;
    for (std::size_t index = 0; index < source.size();) {
        const char ch = source[index];

        if (ch == '/' && index + 1 < source.size() && source[index + 1] == '/') {
            while (index < source.size() && source[index] != '\n') {
                ++index;
            }
            continue;
        }

        if (ch == '"') {
            ++index;
            while (index < source.size()) {
                const char stringChar = source[index++];
                if (stringChar == '"') {
                    break;
                }
            }
            continue;
        }

        if (ch == '{') {
            ++braceDepth;
            ++index;
            continue;
        }
        if (ch == '}') {
            if (braceDepth > 0) {
                --braceDepth;
            }
            ++index;
            continue;
        }

        if (braceDepth == 0 && startsWithImportKeyword(source, index)) {
            return true;
        }
        ++index;
    }
    return false;
}
```

- [ ] **Step 3: Verify import tests**

Run:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R cli_multi_source
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: focused CLI import tests pass. Golden parse and import success fixtures pass except import-error fixture stderr may need path spelling refresh because cycle diagnostics use canonical paths.

- [ ] **Step 4: Refresh import-error path goldens if needed**

If `python3 tests/run_golden_tests.py ./build/compiler_design` reports only `tests/golden/import_errors/*.err` path spelling mismatches, run:

```bash
python3 tests/run_golden_tests.py ./build/compiler_design --update
```

Then review only these files:

```bash
git diff -- tests/golden/import_errors tests/golden/import_basic tests/golden/import_nested tests/golden/import_duplicate tests/golden/parse_errors/import_missing_path.* tests/golden/parse_errors/import_missing_semicolon.*
```

Expected: import-error stderr paths match implementation, success fixtures may now also have generated `ast.out`, `ir.out`, or `bytecode.out` if update mode is used. Keep the generated success outputs only if they are useful and correct; at minimum keep `import_basic/ast.out` to prove imports do not appear in AST.

- [ ] **Step 5: Commit SourceManager implementation**

```bash
git add include/SourceManager.hpp src/SourceManager.cpp tests/golden/import_errors tests/golden/import_basic tests/golden/import_nested tests/golden/import_duplicate tests/golden/parse_errors/import_missing_path.* tests/golden/parse_errors/import_missing_semicolon.*
git commit -m "feat: expand source imports"
```

---

### Task 4: Refresh bytecode artifact and Rust VM coverage

**Files:**
- Create: `tests/bytecode_artifacts/import_basic/expected.cdbc`
- Optionally modify: `tests/golden/import_basic/ir.out`
- Optionally modify: `tests/golden/import_basic/bytecode.out`

- [ ] **Step 1: Generate `.cdbc` expected artifact**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --update
```

Expected: `tests/bytecode_artifacts/import_basic/expected.cdbc` is created and `run.out` remains `7\n`.

- [ ] **Step 2: Review generated artifact**

Run:

```bash
git diff -- tests/bytecode_artifacts/import_basic
```

Expected: artifact has a normal function table for `add`, no mention of an import declaration, and main prints `call add(2, 5)`.

- [ ] **Step 3: Verify bytecode artifact and Rust VM import parity**

Run:

```bash
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens
cargo test --manifest-path vm-rs/Cargo.toml
```

Expected: all pass, including `tests/bytecode_artifacts/import_basic`.

- [ ] **Step 4: Commit artifact coverage**

```bash
git add tests/bytecode_artifacts/import_basic tests/golden/import_basic/ir.out tests/golden/import_basic/bytecode.out
git commit -m "test: add bytecode import coverage"
```

If `tests/golden/import_basic/ir.out` or `bytecode.out` were not generated, omit them from `git add` and keep the same commit message.

---

### Task 5: Documentation and roadmap updates

**Files:**
- Modify: `README.md`
- Modify: `docs/language-grammar.ebnf`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar**

In `docs/language-grammar.ebnf`, update the declaration grammar to include imports. The declaration section should contain this exact rule shape:

```ebnf
declaration     = importDecl | structDecl | funDecl | letDecl | statement ;
importDecl      = "import", string, ";" ;
```

- [ ] **Step 2: Update README language documentation**

Add this concise subsection near other language feature documentation in `README.md`:

````markdown
### Source imports

A top-level source file can load another source file with:

```cd
import "./lib.cd";
```

Imports are source-loading directives: the imported file is expanded at the import declaration's position before normal parsing, and the final program has one shared top-level scope. Import paths are resolved relative to the file that contains the import. Importing the same canonical file more than once is a no-op, which allows shared helper files to be imported through multiple paths in the source graph.

This phase does not add namespaces, `export`, package search paths, separate compilation, or imports from stdin. `import` inside strings or `//` comments is ignored by the loader.
```
````

- [ ] **Step 3: Update roadmap Phase 14**

In `docs/roadmap.md`, mark Phase 14B as complete with wording like:

```markdown
- [x] Phase 14B: Minimal source-loading imports (`import "path";`) with relative path resolution, duplicate suppression, cycle/missing-file import diagnostics, and bytecode/Rust VM parity coverage.
```

Leave future module namespace/export work unchecked if it exists.

- [ ] **Step 4: Update AGENTS project memory**

In `AGENTS.md`, add this sentence to the current language semantics section:

```markdown
- Top-level `import "path";` directives are expanded by `SourceManager` before lexing/parsing. Paths resolve relative to the importing file, duplicate canonical imports are no-ops, imports from stdin are rejected, and this phase has no namespace/export/private visibility or separate compilation.
```

- [ ] **Step 5: Verify docs-only changes do not break tests**

Run:

```bash
python3 tests/run_golden_tests_selftest.py
```

Expected: selftest passes.

- [ ] **Step 6: Commit docs**

```bash
git add README.md docs/language-grammar.ebnf docs/roadmap.md AGENTS.md
git commit -m "docs: document source imports"
```

---

### Task 6: Full verification, cleanup, push

**Files:**
- No planned source edits.
- Remove generated `tests/__pycache__/` if present.

- [ ] **Step 1: Check status before final verification**

Run:

```bash
git status --short --branch
```

Expected: branch is `master`; no unexpected unrelated files are present.

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

- [ ] **Step 3: Inspect final diff/log**

Run:

```bash
git status --short --branch
git log --oneline --decorate -6
```

Expected: only intentional commits are ahead of `origin/master`; worktree is clean after removing `tests/__pycache__`.

- [ ] **Step 4: Push to master**

Run:

```bash
git push origin master
```

Expected: push succeeds.

- [ ] **Step 5: Final report**

Report these exact items to the user:

```text
完成 Phase 14B source-loading imports 并已 push。

实现：
- `import "path";` 在 SourceManager 中递归展开，路径相对 importing file。
- 重复 canonical import no-op。
- stdin import、missing import、cycle import 使用 `Import error:`。
- malformed import 保持 `Parse error`。
- 覆盖 golden、CLI、bytecode artifact、Rust VM parity。

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

**Spec coverage:** The plan covers syntax, source expansion, shared scope, order preservation, duplicate suppression, CLI multi-file interaction, relative path resolution, stdin rejection, missing-file and cycle diagnostics, malformed import parse errors, import scanner ignoring comments/strings, bytecode/Rust VM coverage, and documentation.

**Placeholder scan:** No placeholder markers, deferred-implementation notes, or unstated test-writing steps remain. The only intentional refresh step is for path spelling in import diagnostics and generated bytecode artifacts, with exact commands and review instructions.

**Type consistency:** `DiagnosticKind::Import`, `TokenType::Import`, `SourceManager::loadFile`, `SourceManager::expandImports`, and `SourceManager::containsTopLevelImportKeyword` names are consistent across declarations and implementation snippets.
