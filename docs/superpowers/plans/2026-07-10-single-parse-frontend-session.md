# Single-Parse Frontend Session Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace handwritten import scanning and double parsing with a `FrontendSession` that parses every input module once while preserving CLI and module behavior.

**Architecture:** `FrontendSession` owns file reading, canonical identities, parse results, import traversal, and parser/lexer diagnostic context. It discovers imports from parsed top-level `ImportStmt` objects, patches their existing `resolvedModuleId` fields, and assembles either ordinary direct-entry statements or the existing `ModuleStmt` representation. `main.cpp` consumes its completed `Program` and stored display tokens, leaving `TypeChecker` and both execution backends unchanged.

**Diagnostic-order decision:** Parser and lexer errors in an importing module take precedence over dependency-loading errors in that same module. This follows AST-driven import discovery and intentionally replaces the legacy handwritten pre-scan order.

**Tech Stack:** C++17, CMake/CTest, existing recursive-descent parser and AST, Python CLI/golden/parity tests, Rust VM parity tests.

---

## File Structure

- Create: `include/FrontendSession.hpp` — public front-end loading API and private parsed-unit state.
- Create: `src/FrontendSession.cpp` — single-parse loading, import graph traversal, program assembly, and token display merging.
- Create: `tests/frontend_session_tests.cpp` — focused C++ regression test for canonical duplicate imports and AST import IDs.
- Modify: `CMakeLists.txt` — compile `FrontendSession`, remove legacy compilation units, and register the focused test executable.
- Modify: `src/main.cpp` — replace `SourceManager`/`ModuleProgram` branches and combined-location remapping with `FrontendSession`.
- Modify: `tests/cli_multi_source_tests.py` — cover duplicate canonical import spellings through the public CLI.
- Modify: `docs/roadmap.md` — mark the M0 module-loading cleanup slice implemented and make imported struct methods the next language slice.
- Delete: `include/SourceManager.hpp`, `src/SourceManager.cpp`, `include/ModuleProgram.hpp`, `src/ModuleProgram.cpp` — obsolete scanner, load-result transport, and second parser pass.

### Task 1: Add a focused session regression harness

**Files:**
- Create: `tests/frontend_session_tests.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing C++ session test**

Create `tests/frontend_session_tests.cpp` with a self-contained temporary-directory test. It must create `input.cd` importing `./shared.cd` and `./nested/../shared.cd`, plus `shared.cd` exporting `value`; load it through `FrontendSession`; and assert exactly two loaded modules, two import statements, and the same non-sentinel `resolvedModuleId` on both imports.

```cpp
#include "Ast.hpp"
#include "FrontendSession.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "compiler_frontend_session_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "shared.cd") << "let value = 1;\nexport value;\n";
    std::ofstream(root / "input.cd")
        << "import \"./shared.cd\";\nimport \"./nested/../shared.cd\";\nprint value;\n";

    FrontendSession session;
    Program program = session.loadFiles({(root / "input.cd").string()});
    assert(session.moduleCount() == 2);
    assert(program.statements.size() == 2);
    const auto* entry = dynamic_cast<const ModuleStmt*>(program.statements[1].get());
    assert(entry != nullptr);
    const auto* first = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* second = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(first != nullptr && second != nullptr);
    assert(first->resolvedModuleId == second->resolvedModuleId);
    std::filesystem::remove_all(root);
}
```

- [ ] **Step 2: Register and run the failing test**

Add the test target after `compiler_design` in `CMakeLists.txt`; compile only the front-end sources it needs so it does not link `main.cpp`:

```cmake
add_executable(frontend_session_tests
    tests/frontend_session_tests.cpp
    src/Ast.cpp
    src/Diagnostic.cpp
    src/FrontendSession.cpp
    src/Lexer.cpp
    src/Parser.cpp
)
target_include_directories(frontend_session_tests PRIVATE include)
add_test(NAME frontend_session COMMAND frontend_session_tests)
```

Run: `cmake -S . -B build && cmake --build build --target frontend_session_tests`

Expected: CMake configuration fails because `src/FrontendSession.cpp` does not yet exist; this confirms the new test target is wired to the planned implementation boundary.

- [ ] **Step 3: Commit the failing-test scaffold**

```bash
git add CMakeLists.txt tests/frontend_session_tests.cpp
git commit -m "test: cover canonical duplicate module imports"
```

### Task 2: Implement the single-parse session boundary

**Files:**
- Create: `include/FrontendSession.hpp`
- Create: `src/FrontendSession.cpp`

- [ ] **Step 1: Define the narrow public interface and parsed-unit ownership**

Create `include/FrontendSession.hpp`. Keep `ParsedUnit` private so no checker or backend receives loader internals. `moduleCount()` exists solely for the focused regression test.

```cpp
#pragma once

#include "Ast.hpp"
#include "Token.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

class FrontendSession {
public:
    Program loadStdin(std::istream& input);
    Program loadFiles(const std::vector<std::string>& paths);
    std::vector<Token> displayTokens() const;
    std::size_t moduleCount() const;

private:
    struct ParsedUnit {
        std::size_t id = 0;
        std::string path;
        std::string canonicalPath;
        std::string source;
        std::vector<Token> tokens;
        std::vector<StmtPtr> statements;
        bool isEntry = false;
    };

    void reset();
    std::size_t loadFile(const std::string& path, bool isImport, bool isEntry);
    Program assembleProgram();

    std::vector<ParsedUnit> units_;
    std::unordered_map<std::string, std::size_t> canonicalToUnitId_;
    std::vector<std::string> loadingStack_;
    std::vector<std::size_t> entryUnitIds_;
    bool hasImports_ = false;
};
```

- [ ] **Step 2: Implement read/parse helpers with file diagnostics**

In `src/FrontendSession.cpp`, move the old `readAll`, path normalization, path formatting, and cycle-display helpers into an anonymous namespace. Parse a source once and wrap only located lexer/parser `DiagnosticError` values with the source context:

```cpp
struct ParsedSource {
    std::vector<Token> tokens;
    std::vector<StmtPtr> statements;
};

ParsedSource parseSource(const std::string& path, const std::string& source)
{
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scanTokens();
        Parser parser(tokens);
        Program program = parser.parse();
        return ParsedSource{std::move(tokens), std::move(program.statements)};
    } catch (const FileDiagnosticError&) {
        throw;
    } catch (const DiagnosticError& error) {
        if (error.location()) {
            throw FileDiagnosticError(error, DiagnosticSourceContext{path, source, false});
        }
        throw;
    }
}
```

- [ ] **Step 3: Implement AST-driven recursive import resolution**

Implement `loadFile` so a unit is parsed before its imports are examined. Detect cycles by canonical path before consulting `canonicalToUnitId_`; insert a completed child into that map only after recursively loading its dependencies. For each top-level `ImportStmt`, resolve `path.lexeme` relative to the importer, recursively load it, then assign the returned ID to `resolvedModuleId`.

```cpp
for (const StmtPtr& statement : unit.statements) {
    const auto* import = dynamic_cast<const ImportStmt*>(statement.get());
    if (!import) {
        continue;
    }
    hasImports_ = true;
    const std::filesystem::path requested = normalizedPath.parent_path() / import->path.lexeme;
    const std::size_t importedId = loadFile(requested.string(), true, false);
    const_cast<ImportStmt*>(import)->resolvedModuleId = importedId;
}
```

Use a non-const `StmtPtr&` loop in the final implementation instead of `const_cast`:

```cpp
for (StmtPtr& statement : unit.statements) {
    if (auto* import = dynamic_cast<ImportStmt*>(statement.get())) {
        import->resolvedModuleId = loadFile(
            (normalizedPath.parent_path() / import->path.lexeme).string(), true, false);
    }
}
```

Preserve old messages exactly: imported read failures use `failed to open import: <path>`, entry read failures use `failed to open input file: <path>`, and cycles use `import cycle detected: <chain>`.

- [ ] **Step 4: Assemble the compatible program and token display stream**

Implement the compatible assembly split. With imports, move every stored unit's statements into `ModuleStmt` objects in dependency-first unit-ID order. Without imports, concatenate the stored direct-input token streams using legacy newline separation and parse the resulting stream once; retain its source ranges to remap multi-file parser/type diagnostics. `loadStdin` rejects a parsed top-level `ImportStmt` after parsing and otherwise returns the direct statements.

```cpp
if (hasImports_) {
    for (ParsedUnit& unit : units_) {
        program.statements.push_back(std::make_unique<ModuleStmt>(
            unit.id, unit.path, unit.source, std::move(unit.statements), unit.isEntry));
    }
} else {
    for (const std::size_t id : entryUnitIds_) {
        for (StmtPtr& statement : units_[id].statements) {
            program.statements.push_back(std::move(statement));
        }
    }
}
```

`displayTokens()` must concatenate the stored per-unit token sequences in the same dependency-first order, omit intermediate `EndOfFile` tokens, offset line numbers by the joined-source line span, and append one final `EndOfFile` token. It must not construct a second `Lexer`.

- [ ] **Step 5: Run the focused test and compile the CLI**

Run: `cmake --build build --target frontend_session_tests compiler_design && ctest --test-dir build -R frontend_session --output-on-failure`

Expected: both targets compile and the session test passes. The CLI remains on the legacy loading path until Task 3, so its existing behavior also remains buildable during this intermediate commit.

- [ ] **Step 6: Commit the session implementation**

```bash
git add include/FrontendSession.hpp src/FrontendSession.cpp CMakeLists.txt tests/frontend_session_tests.cpp
git commit -m "refactor: add single-parse frontend session"
```

### Task 3: Route the CLI through `FrontendSession`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Replace legacy includes and helper functions**

Remove `#include "ModuleProgram.hpp"`, `#include "SourceManager.hpp"`, `containsImportToken`, `sourceLineSpan`, and `fileDiagnosticForCombinedLocation`. Add `#include "FrontendSession.hpp"`. Retain the existing CLI argument validation and bytecode/output code.

- [ ] **Step 2: Replace the loading branch**

Replace the `source`, `fileLoadResult`, and `usedModuleProgram` state with a `FrontendSession` and a completed `Program`. Print stored tokens after loading:

```cpp
FrontendSession frontend;
Program program = inputPaths.empty()
    ? frontend.loadStdin(std::cin)
    : frontend.loadFiles(inputPaths);

if (showTokens) {
    for (const Token& token : frontend.displayTokens()) {
        std::cout << token << '\n';
    }
    std::cout << '\n';
}
```

Keep the `FileDiagnosticError` catch unchanged. Simplify the `DiagnosticError` catch so it formats against the current source only for stdin; file-backed lexer/parser diagnostics now reach the first catch and file-backed type diagnostics remain wrapped by `TypeChecker::checkModule`.

- [ ] **Step 3: Build and run direct CLI regression tests**

Run: `cmake --build build --target compiler_design && python3 tests/cli_multi_source_tests.py ./build/compiler_design`

Expected: PASS; this covers stdin rejection, relative imports, direct multi-file scoping, and file-local parse/type diagnostics.

- [ ] **Step 4: Commit CLI integration**

```bash
git add src/main.cpp
git commit -m "refactor: route cli loading through frontend session"
```

### Task 4: Preserve public behavior and remove legacy module assembly

**Files:**
- Modify: `tests/cli_multi_source_tests.py`
- Modify: `CMakeLists.txt`
- Delete: `include/SourceManager.hpp`
- Delete: `src/SourceManager.cpp`
- Delete: `include/ModuleProgram.hpp`
- Delete: `src/ModuleProgram.cpp`

- [ ] **Step 1: Add the public canonical-spelling regression**

Add this test method to `CliMultiSourceTests`. It exercises path canonicalization through the public CLI and ensures duplicate direct imports continue to expose one shared module export.

```python
def test_canonical_duplicate_import_spellings_are_deduplicated(self) -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        (root / "nested").mkdir()
        (root / "shared.cd").write_text("let value = 9;\nexport value;\n", encoding="utf-8")
        (root / "input.cd").write_text(
            'import "./shared.cd";\n'
            'import "./nested/../shared.cd";\n'
            'print value;\n',
            encoding="utf-8",
        )

        completed = self.run_compiler("--run", str(root / "input.cd"))

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "9\\n")
        self.assertEqual(completed.stderr, "")
```

- [ ] **Step 2: Remove obsolete sources and their build references**

Delete the four legacy `SourceManager`/`ModuleProgram` files. In `CMakeLists.txt`, replace their source entries with `src/FrontendSession.cpp` in the `compiler_design` executable. Confirm no live production include remains:

```sh
rg -n "SourceManager|ModuleProgram|buildModuleProgram|scanImportDirectives|containsImportToken" include src CMakeLists.txt
```

Expected: no matches.

- [ ] **Step 3: Run behavior-preservation suites**

Run:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/cli_multi_source_tests.py ./build/compiler_design
python3 tests/run_golden_tests.py ./build/compiler_design --case import
python3 tests/run_golden_tests.py ./build/compiler_design --case module
python3 tests/run_golden_tests.py ./build/compiler_design --case namespace_import
```

Expected: every command passes with no golden updates.

- [ ] **Step 4: Commit legacy removal and regressions**

```bash
git add CMakeLists.txt tests/cli_multi_source_tests.py
git rm include/SourceManager.hpp src/SourceManager.cpp include/ModuleProgram.hpp src/ModuleProgram.cpp
git commit -m "refactor: remove legacy module loading path"
```

### Task 5: Record roadmap completion and perform full verification

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Update the active roadmap**

In the M0 list, change item 2 from a future design task to `Implemented: a single-parse FrontendSession owns parsed ASTs, the import graph, canonical file identities, and file-aware front-end diagnostics.` In the dependency ordering, replace `module-loading cleanup` with `imported struct methods` as the first remaining item; retain the rest of the order unchanged.

- [ ] **Step 2: Run the full repository verification set**

Run exactly:

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
git status --short
```

Expected: all test commands pass; the only remaining tracked change before commit is `docs/roadmap.md`; `tests/__pycache__/` is absent.

- [ ] **Step 3: Commit documentation**

```bash
git add docs/roadmap.md
git commit -m "docs: mark single-parse frontend cleanup complete"
```
