# Parser Recovery Multiple Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Report multiple recoverable parse diagnostics in one compiler run while keeping lexer, import, type, IR, bytecode, and runtime behavior unchanged.

**Architecture:** Keep `ParseError` as the single diagnostic unit and add `ParseErrorList` for parser aggregation. `Parser::parse()` and `Parser::blockStatements()` catch statement/declaration-level parse errors, synchronize to a conservative boundary, and continue; `main.cpp` and `FrontendSession` format aggregate diagnostics using the existing source/snippet format.

**Tech Stack:** C++17 compiler frontend, existing diagnostic formatting, Python golden/CLI tests, CMake/CTest.

---

## File Structure

Create or modify:

- `include/Parser.hpp` — add `ParseErrorList`, parser recovery helpers, and parser-owned error storage.
- `src/Parser.cpp` — implement aggregate parse errors, top-level/block-level recovery, and conservative synchronization.
- `include/Diagnostic.hpp` — add `FileDiagnosticErrorList` for file-aware aggregate diagnostics.
- `src/Diagnostic.cpp` — implement `FileDiagnosticErrorList` storage/access.
- `include/FrontendSession.hpp` — add aggregate direct-diagnostic remapping API.
- `src/FrontendSession.cpp` — map `ParseErrorList` to file-aware aggregate diagnostics for stdin, direct multi-file, and imported files.
- `src/main.cpp` — catch and print aggregate parse diagnostics before existing single-diagnostic catches.
- `tests/golden/parse_errors/multiple_top_level_parse_errors.cd` — new fixture source.
- `tests/golden/parse_errors/multiple_top_level_parse_errors.err` — expected multi-diagnostic stderr.
- `tests/golden/parse_errors/multiple_top_level_parse_errors.exit` — expected exit status.
- `tests/golden/parse_errors/multiple_block_parse_errors.cd` — new fixture source.
- `tests/golden/parse_errors/multiple_block_parse_errors.err` — expected multi-diagnostic stderr.
- `tests/golden/parse_errors/multiple_block_parse_errors.exit` — expected exit status.
- `tests/cli_multi_source_tests.py` — add direct multi-file and imported-file multi-parse diagnostic tests.
- `README.md` — document that parse diagnostics can report multiple syntax errors.

Do not modify:

- `docs/language-grammar.ebnf` — grammar is unchanged.
- IR, bytecode, `.cdbc`, or Rust VM files — backend behavior is unchanged.

---

### Task 1: Add failing multi-parse diagnostic tests

**Files:**
- Create: `tests/golden/parse_errors/multiple_top_level_parse_errors.cd`
- Create: `tests/golden/parse_errors/multiple_top_level_parse_errors.err`
- Create: `tests/golden/parse_errors/multiple_top_level_parse_errors.exit`
- Create: `tests/golden/parse_errors/multiple_block_parse_errors.cd`
- Create: `tests/golden/parse_errors/multiple_block_parse_errors.err`
- Create: `tests/golden/parse_errors/multiple_block_parse_errors.exit`
- Modify: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Add top-level parse-error fixture**

Create `tests/golden/parse_errors/multiple_top_level_parse_errors.cd`:

```cd
print ;
let x = ;
print 1;
```

Create `tests/golden/parse_errors/multiple_top_level_parse_errors.err`:

```text
Parse error at 1:7: expected expression
  print ;
        ^
Parse error at 2:9: expected expression
  let x = ;
          ^
```

Create `tests/golden/parse_errors/multiple_top_level_parse_errors.exit`:

```text
1
```

- [ ] **Step 2: Add block-level parse-error fixture**

Create `tests/golden/parse_errors/multiple_block_parse_errors.cd`:

```cd
fun bad() {
  print ;
  let x = ;
  return 1;
}
```

Create `tests/golden/parse_errors/multiple_block_parse_errors.err`:

```text
Parse error at 2:9: expected expression
    print ;
          ^
Parse error at 3:11: expected expression
    let x = ;
            ^
```

Create `tests/golden/parse_errors/multiple_block_parse_errors.exit`:

```text
1
```

The two source snippet lines in this expected file begin with two formatter spaces plus the two source indentation spaces.

- [ ] **Step 3: Add direct multi-file aggregate parse diagnostic test**

In `tests/cli_multi_source_tests.py`, insert this method before the final `if __name__ == "__main__":` block:

```python
    def test_direct_multi_file_parse_errors_reports_all_file_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            first = root / "first.cd"
            second = root / "second.cd"
            first.write_text("print ;\n", encoding="utf-8")
            second.write_text("let x = ;\n", encoding="utf-8")

            completed = self.run_compiler(str(first), str(second))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Parse error at {first}:1:7: expected expression\n"
                "  print ;\n"
                "        ^\n"
                f"Parse error at {second}:1:9: expected expression\n"
                "  let x = ;\n"
                "          ^\n",
            )
```

- [ ] **Step 4: Add imported-file aggregate parse diagnostic test**

In `tests/cli_multi_source_tests.py`, insert this method after the direct multi-file test from Step 3:

```python
    def test_imported_file_parse_errors_reports_all_diagnostics_with_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            input_file = root / "input.cd"
            lib = root / "lib.cd"
            input_file.write_text('import "./lib.cd";\nprint 1;\n', encoding="utf-8")
            lib.write_text("print ;\nlet x = ;\n", encoding="utf-8")

            completed = self.run_compiler(str(input_file))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                f"Parse error at {lib}:1:7: expected expression\n"
                "  print ;\n"
                "        ^\n"
                f"Parse error at {lib}:2:9: expected expression\n"
                "  let x = ;\n"
                "          ^\n",
            )
```

- [ ] **Step 5: Run tests and verify they fail for the expected reason**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case multiple_top_level_parse_errors --case multiple_block_parse_errors
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: both commands fail because the current parser reports only the first parse diagnostic. The failure output should show missing second diagnostics, not syntax errors in the test files.

- [ ] **Step 6: Commit failing tests**

```sh
git add tests/golden/parse_errors/multiple_top_level_parse_errors.cd \
        tests/golden/parse_errors/multiple_top_level_parse_errors.err \
        tests/golden/parse_errors/multiple_top_level_parse_errors.exit \
        tests/golden/parse_errors/multiple_block_parse_errors.cd \
        tests/golden/parse_errors/multiple_block_parse_errors.err \
        tests/golden/parse_errors/multiple_block_parse_errors.exit \
        tests/cli_multi_source_tests.py
git commit -m "test: add parser recovery diagnostics coverage"
```

---

### Task 2: Implement parser aggregate errors and recovery for pathless diagnostics

**Files:**
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add `ParseErrorList` and parser recovery declarations**

In `include/Parser.hpp`, add `#include <exception>` and `#include <optional>` with the existing includes if they are not already present:

```cpp
#include <exception>
#include <optional>
```

After `class ParseError`, add:

```cpp
class ParseErrorList final : public std::exception {
public:
    explicit ParseErrorList(std::vector<ParseError> errors);

    const std::vector<ParseError>& errors() const;
    const char* what() const noexcept override;

private:
    std::vector<ParseError> errors_;
};
```

In the private section of `class Parser`, add these declarations before `declaration()`:

```cpp
    void recordParseError(ParseError error);
    void synchronize(bool stopAtRightBrace);
    std::optional<StmtPtr> parseDeclarationRecovering(bool stopAtRightBrace);
```

Add this private member near `current_`:

```cpp
    std::vector<ParseError> errors_;
```

- [ ] **Step 2: Implement `ParseErrorList`**

In `src/Parser.cpp`, after the existing `ParseError::ParseError` constructor, add:

```cpp
ParseErrorList::ParseErrorList(std::vector<ParseError> errors)
    : errors_(std::move(errors))
{
}

const std::vector<ParseError>& ParseErrorList::errors() const
{
    return errors_;
}

const char* ParseErrorList::what() const noexcept
{
    return "parse errors";
}
```

- [ ] **Step 3: Change `Parser::parse()` to recover and aggregate**

Replace the current `Parser::parse()` body with:

```cpp
Program Parser::parse()
{
    Program program;
    errors_.clear();

    while (!isAtEnd()) {
        if (std::optional<StmtPtr> statement = parseDeclarationRecovering(false)) {
            program.statements.push_back(std::move(*statement));
        }
    }

    if (!errors_.empty()) {
        throw ParseErrorList(std::move(errors_));
    }

    return program;
}
```

- [ ] **Step 4: Implement recovery helpers**

Add these methods after `Parser::parse()`:

```cpp
void Parser::recordParseError(ParseError error)
{
    errors_.push_back(std::move(error));
}

std::optional<StmtPtr> Parser::parseDeclarationRecovering(bool stopAtRightBrace)
{
    try {
        return declaration();
    } catch (ParseError error) {
        recordParseError(std::move(error));
        synchronize(stopAtRightBrace);
        return std::nullopt;
    }
}

void Parser::synchronize(bool stopAtRightBrace)
{
    if (check(TokenType::Semicolon)) {
        advance();
    }

    while (!isAtEnd()) {
        if (check(TokenType::RightBrace)) {
            if (!stopAtRightBrace) {
                advance();
            }
            return;
        }

        switch (peek().type) {
        case TokenType::Let:
        case TokenType::Fun:
        case TokenType::Struct:
        case TokenType::Impl:
        case TokenType::Import:
        case TokenType::Export:
        case TokenType::Print:
        case TokenType::If:
        case TokenType::While:
        case TokenType::For:
        case TokenType::Break:
        case TokenType::Continue:
        case TokenType::Return:
        case TokenType::LeftBrace:
            return;
        default:
            advance();
            break;
        }
    }
}
```

- [ ] **Step 5: Change `blockStatements()` to recover inside blocks**

Replace the loop in `Parser::blockStatements()`:

```cpp
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        statements.push_back(declaration());
    }
```

with:

```cpp
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        if (std::optional<StmtPtr> statement = parseDeclarationRecovering(true)) {
            statements.push_back(std::move(*statement));
        }
    }
```

Keep the existing final `consume(TokenType::RightBrace, "expected `}` after block");` line unchanged.

- [ ] **Step 6: Add pathless aggregate catch in `main.cpp`**

Add `#include "Parser.hpp"` near the other includes in `src/main.cpp`:

```cpp
#include "Parser.hpp"
```

Add this helper inside the anonymous namespace after `printUsage`:

```cpp
void printParseErrorList(const ParseErrorList& errors, const std::string& source)
{
    bool first = true;
    for (const ParseError& error : errors.errors()) {
        if (!first) {
            std::cerr << '\n';
        }
        first = false;
        std::cerr << formatDiagnosticWithSource(error, source);
    }
    std::cerr << '\n';
}
```

Add this catch block before the existing `catch (const FileDiagnosticError& error)` block:

```cpp
    } catch (const ParseErrorList& errors) {
        printParseErrorList(errors, frontend.sourceForDiagnostics());
        return 1;
```

- [ ] **Step 7: Run targeted pathless parse recovery tests**

Run:

```sh
cmake --build build --target compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --case multiple_top_level_parse_errors --case multiple_block_parse_errors
```

Expected: both new parse-error golden fixtures pass.

- [ ] **Step 8: Commit parser recovery for pathless diagnostics**

```sh
git add include/Parser.hpp src/Parser.cpp src/main.cpp
git commit -m "feat: recover multiple parser diagnostics"
```

---

### Task 3: Add file-aware aggregate parse diagnostics

**Files:**
- Modify: `include/Diagnostic.hpp`
- Modify: `src/Diagnostic.cpp`
- Modify: `include/FrontendSession.hpp`
- Modify: `src/FrontendSession.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add `FileDiagnosticErrorList` declaration**

In `include/Diagnostic.hpp`, add `#include <vector>` with the other includes:

```cpp
#include <vector>
```

After `class FileDiagnosticError`, add:

```cpp
class FileDiagnosticErrorList final : public std::exception {
public:
    explicit FileDiagnosticErrorList(std::vector<FileDiagnosticError> errors);

    const std::vector<FileDiagnosticError>& errors() const;
    const char* what() const noexcept override;

private:
    std::vector<FileDiagnosticError> errors_;
};
```

- [ ] **Step 2: Implement `FileDiagnosticErrorList`**

In `src/Diagnostic.cpp`, after `FileDiagnosticError::sourceContext()`, add:

```cpp
FileDiagnosticErrorList::FileDiagnosticErrorList(std::vector<FileDiagnosticError> errors)
    : errors_(std::move(errors))
{
}

const std::vector<FileDiagnosticError>& FileDiagnosticErrorList::errors() const
{
    return errors_;
}

const char* FileDiagnosticErrorList::what() const noexcept
{
    return "diagnostic errors";
}
```

- [ ] **Step 3: Add direct aggregate remapping API**

In `include/FrontendSession.hpp`, add `#include "Parser.hpp"` because the public remapping method references `ParseErrorList`:

```cpp
#include "Parser.hpp"
```

Add this public method next to `remapDirectDiagnostic`:

```cpp
    std::optional<FileDiagnosticErrorList> remapDirectDiagnostics(const ParseErrorList& errors) const;
```

- [ ] **Step 4: Add helper functions in `FrontendSession.cpp`**

In `src/FrontendSession.cpp`, add these helpers immediately before the existing
`ParsedSource parseSource(...)` function:

```cpp
FileDiagnosticError fileDiagnosticFromError(
    const DiagnosticError& error,
    const std::string& path,
    const std::string& source,
    bool pathlessDiagnostics)
{
    return FileDiagnosticError(
        error,
        DiagnosticSourceContext{path, source, pathlessDiagnostics});
}

FileDiagnosticErrorList fileDiagnosticListFromParseErrors(
    const ParseErrorList& errors,
    const std::string& path,
    const std::string& source,
    bool pathlessDiagnostics)
{
    std::vector<FileDiagnosticError> mapped;
    for (const ParseError& error : errors.errors()) {
        mapped.push_back(fileDiagnosticFromError(error, path, source, pathlessDiagnostics));
    }
    return FileDiagnosticErrorList(std::move(mapped));
}
```

- [ ] **Step 5: Map aggregate errors in `parseSource`**

In `parseSource(...)`, add this catch block before `catch (const FileDiagnosticError&)`:

```cpp
    } catch (const ParseErrorList& errors) {
        throw fileDiagnosticListFromParseErrors(errors, path, source, pathlessDiagnostics);
```

The complete catch sequence should start with `ParseErrorList`, then `FileDiagnosticError`, then `DiagnosticError`.

- [ ] **Step 6: Map aggregate errors in `loadFile` parsing**

In `FrontendSession::loadFile`, inside the parser try/catch around `Parser parser(tokens); Program program = parser.parse();`, add this catch block before `catch (const FileDiagnosticError&)`:

```cpp
        } catch (const ParseErrorList& errors) {
            throw fileDiagnosticListFromParseErrors(
                errors,
                displayPath,
                source,
                !fileDiagnostics && !isImport && !thisUnitHasImport);
```

Keep the existing `FileDiagnosticError` and `DiagnosticError` catches after it.

- [ ] **Step 7: Implement direct multi-file aggregate remapping**

In `src/FrontendSession.cpp`, after `remapDirectDiagnostic(...)`, add:

```cpp
std::optional<FileDiagnosticErrorList> FrontendSession::remapDirectDiagnostics(const ParseErrorList& errors) const
{
    if (directInputs_.size() < 2) {
        return std::nullopt;
    }

    std::vector<FileDiagnosticError> mapped;
    for (const ParseError& error : errors.errors()) {
        if (!error.location()) {
            return std::nullopt;
        }

        std::size_t startLine = 1;
        bool foundInput = false;
        for (const DirectInput& input : directInputs_) {
            const std::size_t span = sourceLineSpan(input.source);
            if (span == 0) {
                continue;
            }

            const std::size_t diagnosticLine = static_cast<std::size_t>(error.location()->line);
            if (diagnosticLine >= startLine && diagnosticLine < startLine + span) {
                DiagnosticError remapped(
                    error.kind(),
                    SourceLocation{
                        static_cast<int>(diagnosticLine - startLine + 1),
                        error.location()->column,
                    },
                    error.message());
                mapped.push_back(FileDiagnosticError(
                    remapped,
                    DiagnosticSourceContext{input.path, input.source, false}));
                foundInput = true;
                break;
            }
            startLine += span;
        }

        if (!foundInput) {
            return std::nullopt;
        }
    }

    return FileDiagnosticErrorList(std::move(mapped));
}
```

- [ ] **Step 8: Use aggregate remapping in `loadFiles` direct combined parse**

In `FrontendSession::loadFiles`, in the try/catch that scans/parses `combinedSource_`, add this catch before `catch (const DiagnosticError& error)`:

```cpp
    } catch (const ParseErrorList& errors) {
        if (const std::optional<FileDiagnosticErrorList> remapped = remapDirectDiagnostics(errors)) {
            throw *remapped;
        }
        throw;
```

- [ ] **Step 9: Catch file-aware aggregates in `main.cpp`**

In the anonymous namespace in `src/main.cpp`, add this helper after `printParseErrorList`:

```cpp
void printFileDiagnosticErrorList(const FileDiagnosticErrorList& errors)
{
    bool first = true;
    for (const FileDiagnosticError& error : errors.errors()) {
        if (!first) {
            std::cerr << '\n';
        }
        first = false;
        std::cerr << formatDiagnosticWithSourceContext(error);
    }
    std::cerr << '\n';
}
```

Add this catch block before the `catch (const ParseErrorList& errors)` block:

```cpp
    } catch (const FileDiagnosticErrorList& errors) {
        printFileDiagnosticErrorList(errors);
        return 1;
```

- [ ] **Step 10: Run file-aware targeted tests**

Run:

```sh
cmake --build build --target compiler_design
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
python3 tests/run_golden_tests.py ./build/compiler_design --case multiple_top_level_parse_errors --case multiple_block_parse_errors
```

Expected: CLI multi-source tests pass, and the new parse-error golden fixtures still pass.

- [ ] **Step 11: Commit file-aware aggregate diagnostics**

```sh
git add include/Diagnostic.hpp src/Diagnostic.cpp include/FrontendSession.hpp src/FrontendSession.cpp src/main.cpp
git commit -m "feat: preserve file context for parse diagnostic lists"
```

---

### Task 4: Verify existing parse-error compatibility and golden runner behavior

**Files:**
- Potentially modify only expected parse-error fixtures if recovery intentionally reveals additional diagnostics in an existing fixture.
- Do not modify `tests/run_golden_tests.py` unless this task proves the runner cannot compare full multi-diagnostic `.err` files.

- [ ] **Step 1: Run all parse-error goldens**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case parse_errors
```

Expected: all parse-error fixtures pass. Existing one-line `.err` files remain compatible when the actual output contains one diagnostic plus source snippet.

- [ ] **Step 2: If existing fixtures fail, inspect before updating**

If Step 1 fails, do not immediately refresh broad goldens. First inspect the
runner output and the working tree:

```sh
git status --short
git diff -- tests/golden/parse_errors
```

If failures are caused by accidental parser behavior changes, fix the parser and
rerun Step 1. If failures are caused by useful extra diagnostics in existing
fixtures, update only the affected fixture from the exact failure name printed
by the runner. For example, if the failing check is
`parse_errors/array_trailing_comma default(ast)`, run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case array_trailing_comma --update
git diff -- tests/golden/parse_errors/array_trailing_comma.err
```

Only keep updates where the additional diagnostics are stable, useful, and
caused by the new recovery behavior. Revert noisy `array_trailing_comma` updates
with:

```sh
git checkout -- tests/golden/parse_errors/array_trailing_comma.err
```

- [ ] **Step 3: Run golden selftests**

Run:

```sh
python3 tests/run_golden_tests_selftest.py
```

Expected: all selftests pass. If they fail because full multi-diagnostic `.err` files are not compared exactly, update `tests/run_golden_tests.py` and its selftests to preserve exact matching for multi-line expected parse errors.

- [ ] **Step 4: Commit compatibility updates if any**

If Task 4 changed tracked files, commit them:

```sh
git add tests/golden/parse_errors tests/run_golden_tests.py tests/run_golden_tests_selftest.py
git commit -m "test: update parse diagnostics expectations"
```

If no files changed, do not create a commit.

---

### Task 5: Document multiple parse diagnostics

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update diagnostics documentation**

Find the diagnostics paragraph in `README.md` that currently begins with:

```markdown
Compiler errors are reported as `Lex`, `Parse`, `Type`, `Compile`, `Import`, or `Runtime` errors.
```

Add this sentence after the paragraph that describes source snippets:

```markdown
When the parser can recover at statement boundaries, a single run may report multiple `Parse` diagnostics before later compiler phases are skipped. Lexer, import, type, compile, and runtime diagnostics still stop at the first reported error.
```

- [ ] **Step 2: Run documentation-adjacent test**

Run:

```sh
ctest --test-dir build --output-on-failure -R cmake_config
```

Expected: `cmake_config` passes.

- [ ] **Step 3: Commit docs**

```sh
git add README.md
git commit -m "docs: document parser multiple diagnostics"
```

---

### Task 6: Final verification and cleanup

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

Expected: clean working tree. If tracked files changed during final verification, inspect and commit them. If only untracked generated files such as `tests/__pycache__/` remain, remove them.

- [ ] **Step 3: Record final verification in the completion response**

The final response must list exact verification commands and results:

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
