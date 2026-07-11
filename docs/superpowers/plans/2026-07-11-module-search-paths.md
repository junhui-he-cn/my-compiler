# Module Search Paths Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add string-based module search paths with `-I DIR` and `--import-path DIR`, extensionless `.cd` fallback, current-file precedence, and shared import/re-export resolution.

**Architecture:** Keep parser, AST, type checking, IR, bytecode, and Rust VM semantics unchanged. Centralize import path classification and candidate generation in `FrontendSession`, then make `main.cpp` collect CLI search paths and pass them to the session before loading sources. Cover behavior with C++ frontend-session tests, Python CLI integration tests, golden fixtures, docs, and full project verification.

**Tech Stack:** C++17, `std::filesystem`, existing `FrontendSession` source loader, Python `unittest` CLI tests, golden fixtures, CMake/CTest, Rust VM parity tests.

---

## File Structure

- Modify `include/FrontendSession.hpp`: add public search-path setter, private import-resolution helper declaration, and stored search path vector.
- Modify `src/FrontendSession.cpp`: add explicit/non-explicit import classification, raw/`.cd` candidate generation, search path iteration, searched-path diagnostics, and shared ordinary import/re-export resolution.
- Modify `src/main.cpp`: parse `-I DIR` and `--import-path DIR`, update usage text, and pass search paths to `FrontendSession`.
- Modify `tests/frontend_session_tests.cpp`: expand from one smoke test into focused source-loader unit tests for canonical duplicates, search-path resolution, re-export resolution, current-file precedence, and explicit-relative no-fallback behavior.
- Modify `tests/cli_multi_source_tests.py`: add VM-backed CLI integration tests for `-I`, `--import-path`, search order, current directory precedence, explicit no-fallback, searched-path diagnostics, missing option arguments, and stdin rejection.
- Create `tests/golden/import_extensionless_current_dir/`: golden run fixture for extensionless import resolved as `lib.cd` in the current directory.
- Create `tests/golden/re_export_extensionless_current_dir/`: golden run fixture for extensionless re-export source resolution in the current directory.
- Create `tests/golden/import_errors/import_missing_extensionless.*`: import-error fixture for searched current-directory candidates without CLI search paths.
- Modify `README.md`: document CLI flags, resolution order, `.cd` fallback, explicit path behavior, re-export resolution, and stdin limitation.
- Modify `docs/roadmap.md`: mark Phase 14F complete by moving the near-term recommendation to module-interface metadata.

---

### Task 1: Add failing FrontendSession search-path tests

**Files:**
- Modify: `tests/frontend_session_tests.cpp`

- [ ] **Step 1: Confirm starting status**

Run:

```sh
git status --short
```

Expected: no output except existing committed-ahead branch metadata from `git status --short --branch` if you use the longer form.

- [ ] **Step 2: Replace `tests/frontend_session_tests.cpp` with expanded tests**

Replace the entire file with:

```cpp
#include "Ast.hpp"
#include "Diagnostic.hpp"
#include "FrontendSession.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string pathString(const fs::path& path)
{
    return path.lexically_normal().generic_string();
}

void writeFile(const fs::path& path, const std::string& source)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    output << source;
}

const ModuleStmt* moduleByPath(const Program& program, const fs::path& path)
{
    const std::string expected = pathString(path);
    for (const StmtPtr& statement : program.statements) {
        const auto* module = dynamic_cast<const ModuleStmt*>(statement.get());
        if (module && module->path == expected) {
            return module;
        }
    }
    assert(false && "expected module path not found");
    return nullptr;
}

void expectImportError(const std::function<void()>& action, const std::string& expectedMessage)
{
    try {
        action();
    } catch (const DiagnosticError& error) {
        assert(error.kind() == DiagnosticKind::Import);
        assert(error.message() == expectedMessage);
        return;
    }
    assert(false && "expected import error");
}

void test_canonical_duplicate_import_spellings_are_deduplicated(const fs::path& root)
{
    fs::remove_all(root);
    fs::create_directories(root / "nested");

    writeFile(root / "shared.cd", "let value = 1;\nexport value;\n");
    writeFile(
        root / "input.cd",
        "import \"./shared.cd\";\n"
        "import \"./nested/../shared.cd\";\n"
        "print value;\n");

    FrontendSession session;
    Program program = session.loadFiles({(root / "input.cd").string()});

    assert(session.moduleCount() == 2);
    assert(program.statements.size() == 2);
    const auto* entry = moduleByPath(program, root / "input.cd");
    const auto* first = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* second = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(first != nullptr && second != nullptr);
    assert(first->resolvedModuleId == second->resolvedModuleId);
}

void test_search_path_resolves_extensionless_import_and_reexport(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(search / "lib.cd", "let value = 7;\nexport value;\n");
    writeFile(app / "api.cd", "export value from \"lib\";\n");
    writeFile(
        app / "input.cd",
        "import \"api\";\n"
        "import \"lib\";\n"
        "print value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    Program program = session.loadFiles({(app / "input.cd").string()});

    assert(session.moduleCount() == 3);
    const auto* lib = moduleByPath(program, search / "lib.cd");
    const auto* api = moduleByPath(program, app / "api.cd");
    const auto* entry = moduleByPath(program, app / "input.cd");

    const auto* reExport = dynamic_cast<const ExportStmt*>(api->statements[0].get());
    assert(reExport != nullptr);
    assert(reExport->resolvedModuleId == lib->moduleId);

    const auto* apiImport = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    const auto* libImport = dynamic_cast<const ImportStmt*>(entry->statements[1].get());
    assert(apiImport != nullptr && libImport != nullptr);
    assert(apiImport->resolvedModuleId == api->moduleId);
    assert(libImport->resolvedModuleId == lib->moduleId);
}

void test_importing_file_directory_precedes_search_path(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(app / "math.cd", "let value = \"local\";\nexport value;\n");
    writeFile(search / "math.cd", "let value = \"search\";\nexport value;\n");
    writeFile(app / "input.cd", "import \"math\";\nprint value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    Program program = session.loadFiles({(app / "input.cd").string()});

    assert(session.moduleCount() == 2);
    const auto* localMath = moduleByPath(program, app / "math.cd");
    const auto* entry = moduleByPath(program, app / "input.cd");
    const auto* import = dynamic_cast<const ImportStmt*>(entry->statements[0].get());
    assert(import != nullptr);
    assert(import->resolvedModuleId == localMath->moduleId);
}

void test_explicit_relative_import_does_not_use_search_path(const fs::path& root)
{
    fs::remove_all(root);
    const fs::path app = root / "app";
    const fs::path search = root / "modules";

    writeFile(search / "missing.cd", "let value = \"search\";\nexport value;\n");
    writeFile(app / "input.cd", "import \"./missing\";\nprint value;\n");

    FrontendSession session;
    session.setImportSearchPaths({search.string()});
    expectImportError(
        [&]() { session.loadFiles({(app / "input.cd").string()}); },
        "failed to open import: " + pathString(app / "missing"));
}

} // namespace

int main()
{
    const fs::path root = fs::temp_directory_path() / "compiler_frontend_session_test";

    test_canonical_duplicate_import_spellings_are_deduplicated(root / "duplicates");
    test_search_path_resolves_extensionless_import_and_reexport(root / "search_reexport");
    test_importing_file_directory_precedes_search_path(root / "precedence");
    test_explicit_relative_import_does_not_use_search_path(root / "explicit_no_fallback");

    fs::remove_all(root);
}
```

- [ ] **Step 3: Build the focused test and confirm it fails before implementation**

Run:

```sh
cmake --build build --target frontend_session_tests
```

Expected: compilation fails because `FrontendSession` has no member named `setImportSearchPaths`.

---

### Task 2: Implement FrontendSession import resolution

**Files:**
- Modify: `include/FrontendSession.hpp`
- Modify: `src/FrontendSession.cpp`
- Test: `tests/frontend_session_tests.cpp`

- [ ] **Step 1: Add search-path API and resolver declarations**

In `include/FrontendSession.hpp`, add `<filesystem>` to the includes:

```cpp
#include <cstddef>
#include <filesystem>
#include <iosfwd>
```

Add this public method before `loadStdin`:

```cpp
    void setImportSearchPaths(std::vector<std::string> paths);
```

Add this private struct after `DirectInput`:

```cpp
    struct ImportResolution {
        std::filesystem::path path;
        std::vector<std::string> triedDisplayPaths;
    };
```

Add this private method declaration after `loadFile(...)`:

```cpp
    ImportResolution resolveImportPath(const std::filesystem::path& importingPath, const Token& pathToken) const;
```

Add this field before `combinedSource_`:

```cpp
    std::vector<std::filesystem::path> importSearchPaths_;
```

Do not clear `importSearchPaths_` in `reset()`. Search path configuration must survive `loadFiles()` and `loadStdin()` because both call `reset()`.

- [ ] **Step 2: Add resolver helpers in `src/FrontendSession.cpp`**

In the anonymous namespace, after `importPath(const Token& token)`, add:

```cpp
bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

bool isExplicitImportPath(const std::string& value)
{
    const std::filesystem::path path(value);
    return path.is_absolute() || startsWith(value, "./") || startsWith(value, "../");
}

bool canOpenFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return input.good();
}

std::vector<std::filesystem::path> importCandidatesForBase(
    const std::filesystem::path& base,
    const std::filesystem::path& requested)
{
    std::filesystem::path raw = requested.is_absolute()
        ? requested
        : base / requested;
    raw = raw.lexically_normal();

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(raw);
    if (requested.extension().empty()) {
        std::filesystem::path withCdExtension = raw;
        withCdExtension += ".cd";
        candidates.push_back(withCdExtension.lexically_normal());
    }
    return candidates;
}

std::string joinDisplayPaths(const std::vector<std::string>& paths)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index != 0) {
            output << ", ";
        }
        output << paths[index];
    }
    return output.str();
}
```

- [ ] **Step 3: Add `setImportSearchPaths` implementation**

After `FrontendSession::reset()`, add:

```cpp
void FrontendSession::setImportSearchPaths(std::vector<std::string> paths)
{
    importSearchPaths_.clear();
    for (const std::string& path : paths) {
        importSearchPaths_.push_back(std::filesystem::path(path).lexically_normal());
    }
}
```

- [ ] **Step 4: Add shared resolver implementation**

After `setImportSearchPaths(...)`, add:

```cpp
FrontendSession::ImportResolution FrontendSession::resolveImportPath(
    const std::filesystem::path& importingPath,
    const Token& pathToken) const
{
    const std::string requestedText = importPath(pathToken);
    const std::filesystem::path requestedPath(requestedText);
    const bool explicitPath = isExplicitImportPath(requestedText);

    std::vector<std::filesystem::path> bases;
    if (requestedPath.is_absolute()) {
        bases.emplace_back();
    } else {
        bases.push_back(importingPath.parent_path());
    }
    if (!explicitPath) {
        bases.insert(bases.end(), importSearchPaths_.begin(), importSearchPaths_.end());
    }

    std::vector<std::string> triedDisplayPaths;
    for (const std::filesystem::path& base : bases) {
        for (const std::filesystem::path& candidate : importCandidatesForBase(base, requestedPath)) {
            const std::string displayPath = pathString(candidate);
            triedDisplayPaths.push_back(displayPath);
            if (canOpenFile(candidate)) {
                return ImportResolution{candidate, std::move(triedDisplayPaths)};
            }
        }
    }

    if (explicitPath) {
        const std::string displayPath = triedDisplayPaths.empty()
            ? requestedText
            : triedDisplayPaths.front();
        throw DiagnosticError(DiagnosticKind::Import, "failed to open import: " + displayPath);
    }

    throw DiagnosticError(
        DiagnosticKind::Import,
        "failed to resolve import `" + requestedText + "`; tried: " + joinDisplayPaths(triedDisplayPaths));
}
```

- [ ] **Step 5: Use the resolver for ordinary imports**

In `FrontendSession::loadFile(...)`, replace this ordinary import block:

```cpp
            if (auto* import = dynamic_cast<ImportStmt*>(statement.get())) {
                hasImports_ = true;
                const std::filesystem::path resolvedPath = normalizedPath.parent_path() / importPath(import->path);
                import->resolvedModuleId = loadFile(resolvedPath.string(), true, false, true);
                continue;
            }
```

with:

```cpp
            if (auto* import = dynamic_cast<ImportStmt*>(statement.get())) {
                hasImports_ = true;
                const ImportResolution resolution = resolveImportPath(normalizedPath, import->path);
                import->resolvedModuleId = loadFile(resolution.path.string(), true, false, true);
                continue;
            }
```

- [ ] **Step 6: Use the resolver for re-export source clauses**

In `FrontendSession::loadFile(...)`, replace this re-export source block:

```cpp
            hasImports_ = true;
            const std::filesystem::path resolvedPath = normalizedPath.parent_path() / importPath(*exportStmt->sourcePath);
            exportStmt->resolvedModuleId = loadFile(resolvedPath.string(), true, false, true);
```

with:

```cpp
            hasImports_ = true;
            const ImportResolution resolution = resolveImportPath(normalizedPath, *exportStmt->sourcePath);
            exportStmt->resolvedModuleId = loadFile(resolution.path.string(), true, false, true);
```

- [ ] **Step 7: Run the focused C++ test**

Run:

```sh
cmake --build build --target frontend_session_tests
./build/frontend_session_tests
```

Expected: build exits 0 and `./build/frontend_session_tests` exits 0 with no output.

- [ ] **Step 8: Run targeted existing import-related Python tests**

Run:

```sh
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
python3 tests/run_golden_tests.py ./build/compiler_design --case import_errors
```

Expected: both commands exit 0. The golden command should report import-error fixtures passing; if existing explicit missing-file fixtures change, restore the explicit-path diagnostic shape before proceeding.

- [ ] **Step 9: Commit resolver implementation**

Run:

```sh
git status --short
git add include/FrontendSession.hpp src/FrontendSession.cpp tests/frontend_session_tests.cpp
git commit -m "feat: resolve imports through search paths"
```

Expected: commit succeeds. Include only the resolver implementation and focused C++ tests in this commit.

---

### Task 3: Add failing CLI integration tests

**Files:**
- Modify: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Add a VM helper that accepts compiler arguments**

In `tests/cli_multi_source_tests.py`, after `emit_and_run_vm(...)`, add:

```python
    def emit_and_run_vm_with_compiler_args(
        self,
        root: Path,
        compiler_args: tuple[str, ...],
        *sources: Path,
    ) -> subprocess.CompletedProcess[str]:
        artifact = root / "program.cdbc"
        emitted = self.run_compiler(
            *compiler_args,
            "--emit-bytecode",
            str(artifact),
            *(str(source) for source in sources),
        )
        self.assertEqual(emitted.returncode, 0, emitted.stderr)
        self.assertEqual(emitted.stdout, "")
        self.assertEqual(emitted.stderr, "")
        self.assertTrue(artifact.is_file())
        return subprocess.run(
            ["cargo", "run", "--quiet", "--manifest-path", str(self.vm_manifest), "--", "run", str(artifact)],
            text=True,
            capture_output=True,
            check=False,
        )
```

- [ ] **Step 2: Add CLI search-path behavior tests**

Before `test_import_error_stays_one_line_without_source_snippet`, add:

```python
    def test_short_import_path_option_resolves_extensionless_module(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            app.mkdir()
            stdlib.mkdir()
            (app / "input.cd").write_text('import "math";\nprint value;\n', encoding="utf-8")
            (stdlib / "math.cd").write_text('let value = "short";\nexport value;\n', encoding="utf-8")

            completed = self.emit_and_run_vm_with_compiler_args(root, ("-I", str(stdlib)), app / "input.cd")

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "short\n")
            self.assertEqual(completed.stderr, "")

    def test_long_import_path_option_resolves_subdirectory_module(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            (stdlib / "pkg").mkdir(parents=True)
            app.mkdir()
            (app / "input.cd").write_text('import "pkg/math";\nprint value;\n', encoding="utf-8")
            (stdlib / "pkg" / "math.cd").write_text('let value = "subdir";\nexport value;\n', encoding="utf-8")

            completed = self.emit_and_run_vm_with_compiler_args(
                root,
                ("--import-path", str(stdlib)),
                app / "input.cd",
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "subdir\n")
            self.assertEqual(completed.stderr, "")

    def test_import_search_paths_are_tried_in_cli_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            first = root / "first"
            second = root / "second"
            app.mkdir()
            first.mkdir()
            second.mkdir()
            (app / "input.cd").write_text('import "lib";\nprint value;\n', encoding="utf-8")
            (first / "lib.cd").write_text('let value = "first";\nexport value;\n', encoding="utf-8")
            (second / "lib.cd").write_text('let value = "second";\nexport value;\n', encoding="utf-8")

            completed = self.emit_and_run_vm_with_compiler_args(
                root,
                ("-I", str(first), "--import-path", str(second)),
                app / "input.cd",
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "first\n")
            self.assertEqual(completed.stderr, "")

    def test_importing_file_directory_wins_over_search_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            app.mkdir()
            stdlib.mkdir()
            (app / "input.cd").write_text('import "lib";\nprint value;\n', encoding="utf-8")
            (app / "lib.cd").write_text('let value = "local";\nexport value;\n', encoding="utf-8")
            (stdlib / "lib.cd").write_text('let value = "search";\nexport value;\n', encoding="utf-8")

            completed = self.emit_and_run_vm_with_compiler_args(root, ("-I", str(stdlib)), app / "input.cd")

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "local\n")
            self.assertEqual(completed.stderr, "")

    def test_explicit_relative_import_does_not_fall_back_to_search_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            app.mkdir()
            stdlib.mkdir()
            (app / "input.cd").write_text('import "./missing";\nprint value;\n', encoding="utf-8")
            (stdlib / "missing.cd").write_text('let value = "search";\nexport value;\n', encoding="utf-8")

            completed = self.run_compiler("-I", str(stdlib), str(app / "input.cd"))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, f"Import error: failed to open import: {app / 'missing'}\n")
            self.assertNotIn(str(stdlib), completed.stderr)

    def test_missing_search_path_import_reports_tried_candidates(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            app = root / "app"
            stdlib = root / "stdlib"
            app.mkdir()
            stdlib.mkdir()
            (app / "input.cd").write_text('import "missing";\n', encoding="utf-8")

            completed = self.run_compiler("-I", str(stdlib), str(app / "input.cd"))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(
                completed.stderr,
                "Import error: failed to resolve import `missing`; tried: "
                f"{app / 'missing'}, {app / 'missing.cd'}, "
                f"{stdlib / 'missing'}, {stdlib / 'missing.cd'}\n",
            )

    def test_import_path_options_require_arguments(self) -> None:
        for option in ("-I", "--import-path"):
            with self.subTest(option=option):
                completed = self.run_compiler(option)

                self.assertEqual(completed.returncode, 64)
                self.assertEqual(completed.stdout, "")
                self.assertIn("Usage:", completed.stderr)

    def test_import_path_does_not_enable_stdin_imports(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "lib.cd").write_text('let value = 1;\nexport value;\n', encoding="utf-8")

            completed = self.run_compiler("-I", str(root), input_text='import "lib";\n')

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")
```

- [ ] **Step 3: Run the CLI test file and confirm it fails before CLI parsing is implemented**

Run:

```sh
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: failures show `-I` or `--import-path` are treated as input files, or missing option arguments return the wrong status. Keep these failures; Task 4 fixes them.

---

### Task 4: Implement CLI option parsing and usage text

**Files:**
- Modify: `src/main.cpp`
- Test: `tests/cli_multi_source_tests.py`

- [ ] **Step 1: Update usage text**

Replace `printUsage(...)` with:

```cpp
void printUsage(const char* executable)
{
    std::cerr << "Usage: " << executable << " [--tokens] [--ir] [--bytecode] [-I dir] [--import-path dir] [file ...]\n"
              << "       " << executable << " [--emit-bytecode output.cdbc] [-I dir] [--import-path dir] file [...]\n"
              << "If no file is provided, source is read from stdin except for --emit-bytecode, which requires at least one file.\n"
              << "Import search paths are used for non-explicit string imports after the importing file's directory.\n";
}
```

- [ ] **Step 2: Collect import search paths during argument parsing**

In `main(...)`, after `std::vector<std::string> inputPaths;`, add:

```cpp
    std::vector<std::string> importSearchPaths;
```

Inside the argument loop, after the `--bytecode` branch and before the `--run` branch, add:

```cpp
        } else if (arg == "-I" || arg == "--import-path") {
            if (i + 1 >= argc) {
                printUsage(argv[0]);
                return 64;
            }
            importSearchPaths.push_back(argv[++i]);
```

- [ ] **Step 3: Pass search paths to `FrontendSession`**

After `FrontendSession frontend;`, add:

```cpp
    frontend.setImportSearchPaths(importSearchPaths);
```

- [ ] **Step 4: Rebuild and run CLI tests**

Run:

```sh
cmake --build build
python3 tests/cli_multi_source_tests.py ./build/compiler_design vm-rs
```

Expected: build exits 0 and `cli_multi_source_tests.py` exits 0.

- [ ] **Step 5: Commit CLI implementation**

Run:

```sh
git status --short
git add src/main.cpp tests/cli_multi_source_tests.py
git commit -m "feat: add import search path CLI options"
```

Expected: commit succeeds. Include only CLI parsing/usage and Python CLI integration tests in this commit.

---

### Task 5: Add golden fixtures and documentation

**Files:**
- Create: `tests/golden/import_extensionless_current_dir/input.cd`
- Create: `tests/golden/import_extensionless_current_dir/lib.cd`
- Create: `tests/golden/import_extensionless_current_dir/run.out`
- Create: `tests/golden/re_export_extensionless_current_dir/input.cd`
- Create: `tests/golden/re_export_extensionless_current_dir/api.cd`
- Create: `tests/golden/re_export_extensionless_current_dir/lib.cd`
- Create: `tests/golden/re_export_extensionless_current_dir/run.out`
- Create: `tests/golden/import_errors/import_missing_extensionless.cd`
- Create: `tests/golden/import_errors/import_missing_extensionless.err`
- Create: `tests/golden/import_errors/import_missing_extensionless.exit`
- Modify: `README.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Add extensionless current-directory import fixture**

Create `tests/golden/import_extensionless_current_dir/lib.cd`:

```text
let value = "extensionless";
export value;
```

Create `tests/golden/import_extensionless_current_dir/input.cd`:

```text
import "lib";
print value;
```

Create `tests/golden/import_extensionless_current_dir/run.out`:

```text
extensionless
```

- [ ] **Step 2: Add extensionless current-directory re-export fixture**

Create `tests/golden/re_export_extensionless_current_dir/lib.cd`:

```text
let value = "barrel";
export value;
```

Create `tests/golden/re_export_extensionless_current_dir/api.cd`:

```text
export value from "lib";
```

Create `tests/golden/re_export_extensionless_current_dir/input.cd`:

```text
import "api";
print value;
```

Create `tests/golden/re_export_extensionless_current_dir/run.out`:

```text
barrel
```

- [ ] **Step 3: Add missing extensionless import-error fixture**

Create `tests/golden/import_errors/import_missing_extensionless.cd`:

```text
import "missing";
```

Create `tests/golden/import_errors/import_missing_extensionless.err`:

```text
Import error: failed to resolve import `missing`; tried: <repo>/tests/golden/import_errors/missing, <repo>/tests/golden/import_errors/missing.cd
```

Create `tests/golden/import_errors/import_missing_extensionless.exit`:

```text
1
```

- [ ] **Step 4: Update README source import documentation**

In `README.md`, replace this paragraph:

```md
Import paths are resolved relative to the file that contains the import.
Imported files have module-private top-level scope. Only declarations marked
with `export` are introduced into the importing file's top-level scope:
```

with:

````md
Import paths are resolved relative to the file that contains the import. The
CLI can also add search directories for non-explicit import strings:

```sh
compiler_design -I stdlib --import-path vendor main.cd
```

For `import "math";`, the loader first tries `math` and then `math.cd` next to
the importing file. If neither exists, it tries the same raw and `.cd` candidates
under each `-I` or `--import-path` directory in command-line order. Paths that
start with `./`, `../`, or an absolute root are explicit paths; they use the
importing file's directory and the `.cd` fallback, but they do not fall back to
CLI search paths. Re-export source clauses use the same resolution rules.

Imported files have module-private top-level scope. Only declarations marked
with `export` are introduced into the importing file's top-level scope:
````

Later in the same section, replace:

```md
metadata for direct and namespace importers. The module system does not add
renaming re-exports, wildcard exports, package search paths, separate
compilation, or imports from stdin. `import` inside strings or `//` comments is
ignored by the loader.
```

with:

```md
metadata for direct and namespace importers. The module system does not add
renaming re-exports, wildcard exports, package manifests, import maps, separate
compilation, or imports from stdin. `import` inside strings or `//` comments is
ignored by the loader.
```

- [ ] **Step 5: Update roadmap to move past Phase 14F**

In `docs/roadmap.md`, update the M2 section so it lists only module-interface metadata as active M2 work:

```md
### M2: Module Ergonomics

1. Define the module-interface metadata needed by future separate compilation,
   but do not implement a linker or separate compilation yet.
```

Update the immediate dependency order block to:

```text
module-interface metadata
```

In `## Phase 14: Modules / Imports`, replace the future-work bullets with:

```md
Future work:

- Stable module-interface metadata needed by possible future separate
  compilation. Do not implement a linker or separate compilation in the
  near-term roadmap.
```

Replace the recommended split list with:

```md
Recommended split:

- Phase 14G: define stable module-interface metadata needed by possible future
  separate compilation. Treat linker or separate-compilation implementation as a
  dedicated compiler/backend effort.
```

Update the near-term recommendation block to:

```text
Phase 14G module-interface metadata
```

- [ ] **Step 6: Run targeted golden and Rust VM checks**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design --case import_extensionless_current_dir --case re_export_extensionless_current_dir --case import_missing_extensionless
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens --case import_extensionless_current_dir --case re_export_extensionless_current_dir
```

Expected: both commands exit 0. The golden command should include the new import-error fixture and report 0 failures. The Rust VM command should emit and run both new success fixtures.

- [ ] **Step 7: Commit docs and fixtures**

Run:

```sh
git status --short
git add README.md docs/roadmap.md tests/golden/import_extensionless_current_dir tests/golden/re_export_extensionless_current_dir tests/golden/import_errors/import_missing_extensionless.cd tests/golden/import_errors/import_missing_extensionless.err tests/golden/import_errors/import_missing_extensionless.exit
git commit -m "docs: document module search paths"
```

Expected: commit succeeds. Include only docs and golden fixtures in this commit.

---

### Task 6: Final verification and cleanup

**Files:**
- Verify all changed files from Tasks 1-5.
- Remove generated `tests/__pycache__/` if present.

- [ ] **Step 1: Run the full project verification suite**

Run from the repository root:

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

Expected:

- CMake configure exits 0.
- C++ build exits 0.
- CTest reports `100% tests passed`.
- Golden runner reports 0 failures.
- Golden runner selftest reports `OK`.
- Bytecode artifact tests report 0 failures.
- Rust VM tests report 0 failures.
- Cargo reports all Rust tests passed.
- `tests/__pycache__/` is removed.

- [ ] **Step 2: Inspect final git status**

Run:

```sh
git status --short --branch
```

Expected: clean working tree, with the branch ahead of `origin/master` by the commits made for this feature.

- [ ] **Step 3: Review final diff against the feature start**

Run:

```sh
git log --oneline --decorate --max-count=8
git diff origin/master...HEAD --stat
```

Expected: commits are focused: resolver/tests, CLI/tests, docs/fixtures. No build outputs, `tests/__pycache__/`, or unrelated files appear.

- [ ] **Step 4: Report verification evidence**

In the final response, include the exact commands run in Step 1 and their results. Do not claim completion unless Step 1 exits 0 and Step 2 shows no uncommitted changes.
