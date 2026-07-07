# Module Exports Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `export` declarations and module-private imported file scopes while preserving existing `import "path";` loading, bytecode emission, and Rust VM parity.

**Architecture:** Introduce explicit AST wrappers for `import`, `export`, and loaded source modules. Extend `SourceManager` to return structured source units and import edges, parse each loaded file independently, then assemble a module-aware `Program` whose `ModuleStmt` nodes preserve privacy boundaries. Update type checking to process module scopes and exported bindings before IR lowering; IR/bytecode/Rust VM remain unchanged except that IR compilation skips compile-time wrappers.

**Tech Stack:** C++17 compiler front end/interpreter, CMake, Python golden tests, Rust VM integration tests.

---

## File Structure

- `include/Token.hpp`, `src/Lexer.cpp`: add `export` keyword/token.
- `include/Ast.hpp`, `src/Ast.cpp`: add `ImportStmt`, `ExportStmt`, and `ModuleStmt`; update tree and compact printers.
- `include/Parser.hpp`, `src/Parser.cpp`: parse imports as AST nodes, parse export wrappers, and reject invalid/nested exports.
- `include/SourceManager.hpp`, `src/SourceManager.cpp`: add structured `SourceLoadResult`, source units, import edges, and compatibility combined source.
- `include/ModuleProgram.hpp`, `src/ModuleProgram.cpp`: new focused assembly layer that lexes/parses loaded units, patches import edges to module ids, and builds one module-aware `Program`.
- `include/TypeChecker.hpp`, `src/TypeChecker.cpp`: add module export tables, imported binding insertion, and wrapper handling.
- `src/IRCompiler.cpp`: skip `ModuleStmt`, `ImportStmt`, and unwrap `ExportStmt`.
- `src/main.cpp`: use structured loading for file inputs and the existing single-source path for stdin.
- `CMakeLists.txt`: compile `src/ModuleProgram.cpp`.
- `tests/golden/`, `tests/bytecode_artifacts/`, `tests/run_rust_vm_tests.py`: add coverage for exports, private names, conflicts, and parity.
- `docs/language-grammar.ebnf`, `README.md`, `docs/roadmap.md`, `AGENTS.md`: document implemented Phase 14C behavior.

---

### Task 1: Add RED fixtures for module exports

**Files:**
- Create: `tests/golden/module_exports_basic/input.cd`
- Create: `tests/golden/module_exports_basic/lib.cd`
- Create: `tests/golden/module_exports_basic/run.out`
- Create: `tests/golden/module_exports_private_helper/input.cd`
- Create: `tests/golden/module_exports_private_helper/lib.cd`
- Create: `tests/golden/module_exports_private_helper/run.out`
- Create: `tests/golden/type_errors/module_private_name.cd`
- Create: `tests/golden/type_errors/module_private_name_lib.cd`
- Create: `tests/golden/type_errors/module_private_name.err`
- Create: `tests/golden/type_errors/module_private_name.exit`
- Create: `tests/golden/type_errors/module_duplicate_export_a.cd`
- Create: `tests/golden/type_errors/module_duplicate_export_b.cd`
- Create: `tests/golden/type_errors/module_duplicate_export_main.cd`
- Create: `tests/golden/type_errors/module_duplicate_export_main.err`
- Create: `tests/golden/type_errors/module_duplicate_export_main.exit`
- Create: `tests/golden/parse_errors/export_invalid_statement.cd`
- Create: `tests/golden/parse_errors/export_invalid_statement.err`
- Create: `tests/golden/parse_errors/export_invalid_statement.exit`
- Create: `tests/golden/parse_errors/export_nested.cd`
- Create: `tests/golden/parse_errors/export_nested.err`
- Create: `tests/golden/parse_errors/export_nested.exit`

- [ ] **Step 1: Create basic export success fixture**

Create `tests/golden/module_exports_basic/input.cd`:

```cd
import "./lib.cd";
print value;
print add(2, 3);
let p: Point = Point { x: 4, y: 5 };
print p.x + p.y;
```

Create `tests/golden/module_exports_basic/lib.cd`:

```cd
export let value = 7;
export fun add(a, b) { return a + b; }
export struct Point { x: number, y: number }
```

Create `tests/golden/module_exports_basic/run.out`:

```text
7
5
9
```

- [ ] **Step 2: Create private helper success fixture**

Create `tests/golden/module_exports_private_helper/input.cd`:

```cd
import "./lib.cd";
print answer();
```

Create `tests/golden/module_exports_private_helper/lib.cd`:

```cd
let secret = 40;
fun inc(x) { return x + 1; }
export fun answer() { return inc(secret) + 1; }
```

Create `tests/golden/module_exports_private_helper/run.out`:

```text
42
```

- [ ] **Step 3: Create private-name type error fixture**

Create `tests/golden/type_errors/module_private_name.cd`:

```cd
import "./module_private_name_lib.cd";
print secret;
```

Create `tests/golden/type_errors/module_private_name_lib.cd`:

```cd
let secret = 1;
export let visible = 2;
```

Create `tests/golden/type_errors/module_private_name.err`:

```text
Type error at 2:7: undefined variable `secret`
```

Create `tests/golden/type_errors/module_private_name.exit`:

```text
1
```

- [ ] **Step 4: Create duplicate imported export type error fixture**

Create `tests/golden/type_errors/module_duplicate_export_a.cd`:

```cd
export let value = 1;
```

Create `tests/golden/type_errors/module_duplicate_export_b.cd`:

```cd
export let value = 2;
```

Create `tests/golden/type_errors/module_duplicate_export_main.cd`:

```cd
import "./module_duplicate_export_a.cd";
import "./module_duplicate_export_b.cd";
print value;
```

Create `tests/golden/type_errors/module_duplicate_export_main.err`:

```text
Type error at 2:1: variable `value` already declared in this scope
```

Create `tests/golden/type_errors/module_duplicate_export_main.exit`:

```text
1
```

- [ ] **Step 5: Create invalid export parse fixtures**

Create `tests/golden/parse_errors/export_invalid_statement.cd`:

```cd
export print 1;
```

Create `tests/golden/parse_errors/export_invalid_statement.err`:

```text
Parse error at 1:8: expected `let`, `fun`, or `struct` after `export`
```

Create `tests/golden/parse_errors/export_invalid_statement.exit`:

```text
1
```

Create `tests/golden/parse_errors/export_nested.cd`:

```cd
{
  export let value = 1;
}
```

Create `tests/golden/parse_errors/export_nested.err`:

```text
Parse error at 2:3: `export` is only allowed at top level
```

Create `tests/golden/parse_errors/export_nested.exit`:

```text
1
```

- [ ] **Step 6: Run targeted goldens and verify RED**

Run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected now: failures mentioning unexpected parse errors for `export` or current include-style private-name leakage. Do not update goldens yet.

- [ ] **Step 7: Commit RED fixtures**

```sh
git add tests/golden/module_exports_basic tests/golden/module_exports_private_helper \
  tests/golden/type_errors/module_private_name* \
  tests/golden/type_errors/module_duplicate_export_* \
  tests/golden/parse_errors/export_invalid_statement.* \
  tests/golden/parse_errors/export_nested.*
git commit -m "test: add module export fixtures"
```

---

### Task 2: Add export/import/module AST and parser support

**Files:**
- Modify: `include/Token.hpp`
- Modify: `src/Lexer.cpp`
- Modify: `include/Ast.hpp`
- Modify: `src/Ast.cpp`
- Modify: `include/Parser.hpp`
- Modify: `src/Parser.cpp`

- [ ] **Step 1: Add `Export` token**

In `include/Token.hpp`, insert `Export` after `Import`:

```cpp
    Import,
    Export,
    Else,
```

In `src/Lexer.cpp`, add the keyword:

```cpp
        {"export", TokenType::Export},
```

Add the token name case:

```cpp
    case TokenType::Export:
        return "Export";
```

- [ ] **Step 2: Add AST statement nodes**

In `include/Ast.hpp`, after `StructDeclStmt`, add:

```cpp
struct ImportStmt final : Stmt {
    ImportStmt(Token keyword, Token path);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    Token path;
    std::size_t resolvedModuleId = static_cast<std::size_t>(-1);
};

struct ExportStmt final : Stmt {
    ExportStmt(Token keyword, StmtPtr declaration);
    void print(std::ostream& out, int indent) const override;

    Token keyword;
    StmtPtr declaration;
};

struct ModuleStmt final : Stmt {
    ModuleStmt(std::size_t moduleId, std::string path, std::vector<StmtPtr> statements, bool isEntry);
    void print(std::ostream& out, int indent) const override;

    std::size_t moduleId;
    std::string path;
    std::vector<StmtPtr> statements;
    bool isEntry = false;
};
```

Add `#include <cstddef>` to `include/Ast.hpp` if it is not already present.

- [ ] **Step 3: Implement AST constructors and tree printing**

In `src/Ast.cpp`, add compact `writeStmt()` cases before `LetStmt`:

```cpp
    if (const auto* module = dynamic_cast<const ModuleStmt*>(&stmt)) {
        out << "(module " << module->moduleId;
        for (const auto& child : module->statements) {
            out << ' ';
            writeStmt(out, *child);
        }
        out << ')';
        return;
    }

    if (const auto* import = dynamic_cast<const ImportStmt*>(&stmt)) {
        out << "(import " << import->path.lexeme << ')';
        return;
    }

    if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&stmt)) {
        out << "(export ";
        writeStmt(out, *exportStmt->declaration);
        out << ')';
        return;
    }
```

Add constructors and verbose printers near other statement implementations:

```cpp
ImportStmt::ImportStmt(Token keyword, Token path)
    : keyword(std::move(keyword))
    , path(std::move(path))
{
}

void ImportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Import " << path.lexeme << "\n";
}

ExportStmt::ExportStmt(Token keyword, StmtPtr declaration)
    : keyword(std::move(keyword))
    , declaration(std::move(declaration))
{
}

void ExportStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Export\n";
    declaration->print(out, indent + 1);
}

ModuleStmt::ModuleStmt(std::size_t moduleId, std::string path, std::vector<StmtPtr> statements, bool isEntry)
    : moduleId(moduleId)
    , path(std::move(path))
    , statements(std::move(statements))
    , isEntry(isEntry)
{
}

void ModuleStmt::print(std::ostream& out, int indent) const
{
    writeIndent(out, indent);
    out << "Module " << moduleId << " " << path << (isEntry ? " entry" : "") << "\n";
    for (const auto& statement : statements) {
        statement->print(out, indent + 1);
    }
}
```

- [ ] **Step 4: Track top-level parser context**

In `include/Parser.hpp`, add declarations:

```cpp
    StmtPtr exportDeclaration();
    StmtPtr exportTargetDeclaration(const Token& exportKeyword);
```

Add a parser field:

```cpp
    int blockDepth_ = 0;
```

In `src/Parser.cpp`, update `declaration()` before import handling:

```cpp
    if (match(TokenType::Export)) {
        if (blockDepth_ != 0) {
            throw ParseError(previous(), "`export` is only allowed at top level");
        }
        return exportDeclaration();
    }
```

- [ ] **Step 5: Parse import declarations as AST nodes**

Replace `Parser::importDeclaration()` with:

```cpp
StmtPtr Parser::importDeclaration()
{
    Token keyword = previous();
    Token path = consume(TokenType::String, "expected import path string");
    consume(TokenType::Semicolon, "expected `;` after import path");
    return std::make_unique<ImportStmt>(std::move(keyword), std::move(path));
}
```

- [ ] **Step 6: Parse export target declarations**

Add to `src/Parser.cpp`:

```cpp
StmtPtr Parser::exportDeclaration()
{
    Token keyword = previous();
    return std::make_unique<ExportStmt>(keyword, exportTargetDeclaration(keyword));
}

StmtPtr Parser::exportTargetDeclaration(const Token& exportKeyword)
{
    if (match(TokenType::Struct)) {
        return structDeclaration();
    }
    if (check(TokenType::Fun) && checkNext(TokenType::Identifier)) {
        advance();
        return functionDeclaration();
    }
    if (match(TokenType::Let)) {
        return letDeclaration();
    }
    throw ParseError(exportKeyword, "expected `let`, `fun`, or `struct` after `export`");
}
```

- [ ] **Step 7: Make nested export parse errors stable**

In `Parser::blockStatement()`, increment/decrement `blockDepth_` around `blockStatements()`:

```cpp
StmtPtr Parser::blockStatement()
{
    ++blockDepth_;
    std::vector<StmtPtr> statements = blockStatements();
    --blockDepth_;
    return std::make_unique<BlockStmt>(std::move(statements));
}
```

If the current `blockStatement()` already has body code, replace only its body with the snippet above.

- [ ] **Step 8: Build and run parse-error fixtures**

Run:

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design --cases export_invalid_statement export_nested
```

If `--cases` is not supported by the runner, run:

```sh
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: the two export parse-error fixtures pass; module success/type-error fixtures still fail.

- [ ] **Step 9: Commit parser support**

```sh
git add include/Token.hpp src/Lexer.cpp include/Ast.hpp src/Ast.cpp include/Parser.hpp src/Parser.cpp
git commit -m "feat: parse export declarations"
```

---

### Task 3: Add structured source loading

**Files:**
- Modify: `include/SourceManager.hpp`
- Modify: `src/SourceManager.cpp`

- [ ] **Step 1: Add load-result structs**

In `include/SourceManager.hpp`, after `SourceFile`, add:

```cpp
struct SourceImport {
    std::string requestedPath;
    std::string canonicalPath;
};

struct SourceUnit {
    std::size_t id = 0;
    std::string path;
    std::string canonicalPath;
    std::string source;
    bool isEntry = false;
    std::vector<SourceImport> imports;
};

struct SourceLoadResult {
    std::string combinedSource;
    std::vector<SourceUnit> units;
    std::vector<std::size_t> entryUnitIds;
};
```

Add public methods:

```cpp
    SourceLoadResult loadStdinUnit(std::istream& input);
    SourceLoadResult loadFileUnits(const std::vector<std::string>& paths);
```

Add private helpers:

```cpp
    std::size_t loadFileUnit(const std::filesystem::path& path, bool isImport, bool isEntry);
    std::vector<SourceImport> scanImportDirectives(const std::string& source, const std::filesystem::path& importingFile);
```

Add field:

```cpp
    std::vector<SourceUnit> units_;
```

- [ ] **Step 2: Reset units in existing load paths**

In `SourceManager::loadStdin()` and `SourceManager::loadFiles()`, add `units_.clear();` next to existing clears.

- [ ] **Step 3: Implement structured stdin loading**

Add to `src/SourceManager.cpp`:

```cpp
SourceLoadResult SourceManager::loadStdinUnit(std::istream& input)
{
    files_.clear();
    units_.clear();
    loadedFiles_.clear();
    loadingStack_.clear();

    std::string source = readAll(input);
    if (containsTopLevelImportKeyword(source)) {
        throw DiagnosticError(DiagnosticKind::Import, "import is not supported from stdin");
    }

    units_.push_back(SourceUnit{0, "<stdin>", "<stdin>", source, true, {}});
    return SourceLoadResult{source, units_, {0}};
}
```

- [ ] **Step 4: Implement import scanning without text expansion**

Add to `src/SourceManager.cpp`:

```cpp
std::vector<SourceImport> SourceManager::scanImportDirectives(
    const std::string& source,
    const std::filesystem::path& importingFile)
{
    std::vector<SourceImport> imports;
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
                if (source[index++] == '"') {
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
            std::string importPath;
            std::size_t afterDirective = index;
            if (parseImportDirective(source, index, importPath, afterDirective)) {
                const std::filesystem::path resolved = importingFile.parent_path() / importPath;
                const std::filesystem::path canonicalPath = normalizedExistingPath(resolved);
                imports.push_back(SourceImport{importPath, pathString(canonicalPath)});
                index = afterDirective;
                continue;
            }
        }
        ++index;
    }
    return imports;
}
```

- [ ] **Step 5: Implement recursive structured file loading**

Add to `src/SourceManager.cpp`:

```cpp
std::size_t SourceManager::loadFileUnit(const std::filesystem::path& path, bool isImport, bool isEntry)
{
    const std::filesystem::path normalizedPath = normalizedExistingPath(path);
    const std::string canonical = pathString(normalizedPath);
    const std::string display = pathString(path);

    auto loaded = loadedFiles_.find(canonical);
    if (loaded != loadedFiles_.end()) {
        for (const SourceUnit& unit : units_) {
            if (unit.canonicalPath == canonical) {
                return unit.id;
            }
        }
    }

    if (std::find(loadingStack_.begin(), loadingStack_.end(), canonical) != loadingStack_.end()) {
        throw DiagnosticError(DiagnosticKind::Import, "import cycle detected: " + displayCycle(loadingStack_, canonical));
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
    std::vector<SourceImport> imports = scanImportDirectives(source, normalizedPath);
    for (const SourceImport& sourceImport : imports) {
        loadFileUnit(sourceImport.canonicalPath, true, false);
    }
    loadingStack_.pop_back();

    const std::size_t id = units_.size();
    units_.push_back(SourceUnit{id, display, canonical, source, isEntry, std::move(imports)});
    loadedFiles_.insert(canonical);
    return id;
}
```

- [ ] **Step 6: Implement structured multi-file loading**

Add to `src/SourceManager.cpp`:

```cpp
SourceLoadResult SourceManager::loadFileUnits(const std::vector<std::string>& paths)
{
    files_.clear();
    units_.clear();
    loadedFiles_.clear();
    loadingStack_.clear();

    std::vector<std::size_t> entryUnitIds;
    for (const std::string& path : paths) {
        entryUnitIds.push_back(loadFileUnit(path, false, true));
    }

    std::string combined;
    for (const SourceUnit& unit : units_) {
        appendWithNewlineSeparation(combined, unit.source);
    }
    return SourceLoadResult{combined, units_, std::move(entryUnitIds)};
}
```

- [ ] **Step 7: Keep old `loadFiles()` compatible**

Replace `SourceManager::loadFiles()` body with:

```cpp
std::string SourceManager::loadFiles(const std::vector<std::string>& paths)
{
    return loadFileUnits(paths).combinedSource;
}
```

Keep the old `loadFile()` and `expandImports()` temporarily until Task 4 switches main; remove dead code only after the full feature passes.

- [ ] **Step 8: Build**

Run:

```sh
cmake --build build
```

Expected: build succeeds. If there are overload/path conversion errors around `loadFileUnit(sourceImport.canonicalPath, ...)`, wrap the string with `std::filesystem::path(sourceImport.canonicalPath)`.

- [ ] **Step 9: Commit structured loader**

```sh
git add include/SourceManager.hpp src/SourceManager.cpp
git commit -m "feat: load structured source units"
```

---

### Task 4: Add module program assembly

**Files:**
- Create: `include/ModuleProgram.hpp`
- Create: `src/ModuleProgram.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`

- [ ] **Step 1: Add module program header**

Create `include/ModuleProgram.hpp`:

```cpp
#pragma once

#include "Ast.hpp"
#include "SourceManager.hpp"

#include <string>

class ModuleAssemblyError final : public DiagnosticError {
public:
    explicit ModuleAssemblyError(std::string message);
};

Program buildModuleProgram(const SourceLoadResult& loadResult);
```

- [ ] **Step 2: Add module program implementation**

Create `src/ModuleProgram.cpp`:

```cpp
#include "ModuleProgram.hpp"

#include "Lexer.hpp"
#include "Parser.hpp"

#include <stdexcept>
#include <unordered_map>
#include <utility>

ModuleAssemblyError::ModuleAssemblyError(std::string message)
    : DiagnosticError(DiagnosticKind::Compile, std::move(message))
{
}

namespace {

ImportStmt* asImport(StmtPtr& statement)
{
    return dynamic_cast<ImportStmt*>(statement.get());
}

std::vector<StmtPtr> parseUnit(const SourceUnit& unit)
{
    Lexer lexer(unit.source);
    Parser parser(lexer.scanTokens());
    Program parsed = parser.parse();
    return std::move(parsed.statements);
}

void patchImports(
    SourceUnit& unit,
    std::vector<StmtPtr>& statements,
    const std::unordered_map<std::string, std::size_t>& canonicalToId)
{
    std::size_t importIndex = 0;
    for (StmtPtr& statement : statements) {
        ImportStmt* import = asImport(statement);
        if (!import) {
            continue;
        }
        if (importIndex >= unit.imports.size()) {
            continue;
        }
        const SourceImport& edge = unit.imports[importIndex++];
        const auto found = canonicalToId.find(edge.canonicalPath);
        if (found == canonicalToId.end()) {
            throw ModuleAssemblyError("internal error: unresolved import " + edge.requestedPath);
        }
        import->resolvedModuleId = found->second;
    }
}

} // namespace

Program buildModuleProgram(const SourceLoadResult& loadResult)
{
    std::unordered_map<std::string, std::size_t> canonicalToId;
    for (const SourceUnit& unit : loadResult.units) {
        canonicalToId.emplace(unit.canonicalPath, unit.id);
    }

    Program program;
    for (SourceUnit unit : loadResult.units) {
        std::vector<StmtPtr> statements = parseUnit(unit);
        patchImports(unit, statements, canonicalToId);
        program.statements.push_back(std::make_unique<ModuleStmt>(
            unit.id,
            unit.path,
            std::move(statements),
            unit.isEntry));
    }
    return program;
}
```

- [ ] **Step 3: Add source file to CMake**

In `CMakeLists.txt`, add `src/ModuleProgram.cpp` after `src/Lexer.cpp`:

```cmake
    src/ModuleProgram.cpp
```

- [ ] **Step 4: Use module assembly in main for file inputs**

In `src/main.cpp`, include the new header:

```cpp
#include "ModuleProgram.hpp"
```

Replace source loading / lexing / parsing block with this exact shape:

```cpp
        SourceManager sourceManager;
        Program program;
        std::vector<Token> tokens;

        if (inputPaths.empty()) {
            source = sourceManager.loadStdin(std::cin);
            Lexer lexer(source);
            tokens = lexer.scanTokens();
            if (showTokens) {
                for (const Token& token : tokens) {
                    std::cout << token << '\n';
                }
                std::cout << '\n';
            }
            Parser parser(tokens);
            program = parser.parse();
        } else {
            SourceLoadResult loadResult = sourceManager.loadFileUnits(inputPaths);
            source = loadResult.combinedSource;
            if (showTokens) {
                Lexer lexer(source);
                tokens = lexer.scanTokens();
                for (const Token& token : tokens) {
                    std::cout << token << '\n';
                }
                std::cout << '\n';
            }
            program = buildModuleProgram(loadResult);
        }
```

Keep the existing `TypeChecker`, AST printing, IR, bytecode, and run code after this block unchanged.

- [ ] **Step 5: Build and inspect AST output**

Run:

```sh
cmake --build build
./build/compiler_design tests/golden/module_exports_private_helper/input.cd
```

Expected: AST contains `Module` and `Export` nodes. Type checking may still fail because wrappers are unsupported.

- [ ] **Step 6: Commit module assembly**

```sh
git add include/ModuleProgram.hpp src/ModuleProgram.cpp CMakeLists.txt src/main.cpp
git commit -m "feat: assemble module-aware programs"
```

---

### Task 5: Type-check module scopes and exports

**Files:**
- Modify: `include/TypeChecker.hpp`
- Modify: `src/TypeChecker.cpp`

- [ ] **Step 1: Extend binding metadata and module state**

In `include/TypeChecker.hpp`, add `#include <unordered_set>`.

In `Binding`, add:

```cpp
        bool imported = false;
```

Add a module export alias and state fields:

```cpp
    using ExportTable = std::unordered_map<std::string, Binding>;

    std::unordered_map<std::size_t, ExportTable> moduleExports_;
    std::unordered_set<std::size_t> checkedModules_;
    std::vector<std::size_t> moduleStack_;
```

Add private methods:

```cpp
    void checkModule(const ModuleStmt& module);
    void checkImport(const ImportStmt& statement);
    void checkExport(const ExportStmt& statement);
    Binding declareImportedVariable(const Token& name, const Binding& importedBinding);
    const ModuleStmt* findModule(const Program& program, std::size_t moduleId) const;
```

Add field:

```cpp
    const Program* currentProgram_ = nullptr;
```

- [ ] **Step 2: Initialize module state in `check()`**

In `TypeChecker::check()`, reset new fields and store `currentProgram_`:

```cpp
    moduleExports_.clear();
    checkedModules_.clear();
    moduleStack_.clear();
    currentProgram_ = &program;
```

Then replace the current simple statement loop with:

```cpp
    for (const auto& statement : program.statements) {
        if (const auto* module = dynamic_cast<const ModuleStmt*>(statement.get())) {
            checkModule(*module);
        } else {
            checkStatement(*statement);
        }
    }
```

Before returning, set `currentProgram_ = nullptr;` after `endScope();`.

- [ ] **Step 3: Add module lookup helper**

Add to `src/TypeChecker.cpp`:

```cpp
const ModuleStmt* TypeChecker::findModule(const Program& program, std::size_t moduleId) const
{
    for (const auto& statement : program.statements) {
        if (const auto* module = dynamic_cast<const ModuleStmt*>(statement.get())) {
            if (module->moduleId == moduleId) {
                return module;
            }
        }
    }
    return nullptr;
}
```

- [ ] **Step 4: Add imported binding declaration**

Add to `src/TypeChecker.cpp`:

```cpp
TypeChecker::Binding TypeChecker::declareImportedVariable(const Token& name, const Binding& importedBinding)
{
    auto& scope = currentScope();
    if (scope.find(name.lexeme) != scope.end()) {
        throw TypeError(name, "variable `" + name.lexeme + "` already declared in this scope");
    }
    Binding binding = importedBinding;
    binding.imported = true;
    binding.scopeDepth = scopes_.size() - 1;
    scope.emplace(name.lexeme, binding);
    return binding;
}
```

- [ ] **Step 5: Add module checker**

Add to `src/TypeChecker.cpp`:

```cpp
void TypeChecker::checkModule(const ModuleStmt& module)
{
    if (checkedModules_.find(module.moduleId) != checkedModules_.end()) {
        return;
    }

    moduleStack_.push_back(module.moduleId);
    beginScope();

    for (const auto& statement : module.statements) {
        checkStatement(*statement);
    }

    endScope();
    moduleStack_.pop_back();
    checkedModules_.insert(module.moduleId);
}
```

- [ ] **Step 6: Handle imports in type checker**

Add to `src/TypeChecker.cpp`:

```cpp
void TypeChecker::checkImport(const ImportStmt& statement)
{
    if (!currentProgram_) {
        throw TypeError(statement.keyword, "internal error: import without program");
    }
    const ModuleStmt* imported = findModule(*currentProgram_, statement.resolvedModuleId);
    if (!imported) {
        throw TypeError(statement.keyword, "internal error: unresolved import module");
    }
    checkModule(*imported);

    const auto exports = moduleExports_.find(imported->moduleId);
    if (exports == moduleExports_.end()) {
        return;
    }
    for (const auto& entry : exports->second) {
        Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
        declareImportedVariable(name, entry.second);
    }
}
```

- [ ] **Step 7: Handle exports in type checker**

Add to `src/TypeChecker.cpp`:

```cpp
void TypeChecker::checkExport(const ExportStmt& statement)
{
    if (moduleStack_.empty()) {
        throw TypeError(statement.keyword, "`export` is only allowed at top level");
    }

    checkStatement(*statement.declaration);

    std::string exportedName;
    Binding* binding = nullptr;
    if (const auto* let = dynamic_cast<const LetStmt*>(statement.declaration.get())) {
        exportedName = let->name.lexeme;
        binding = findVariable(exportedName);
    } else if (const auto* function = dynamic_cast<const FunctionStmt*>(statement.declaration.get())) {
        exportedName = function->name.lexeme;
        binding = findVariable(exportedName);
    } else if (const auto* structure = dynamic_cast<const StructDeclStmt*>(statement.declaration.get())) {
        exportedName = structure->name.lexeme;
        binding = nullptr;
    } else {
        throw TypeError(statement.keyword, "unsupported export declaration");
    }

    if (!exportedName.empty() && binding) {
        moduleExports_[moduleStack_.back()].emplace(exportedName, *binding);
    }
}
```

This first version exports value/function bindings. Struct export is completed in the next step because struct types live in `structTypes_` rather than normal variable bindings.

- [ ] **Step 8: Export struct types through type checker state**

In `include/TypeChecker.hpp`, add:

```cpp
    std::unordered_map<std::size_t, std::unordered_map<std::string, StructTypeDecl>> moduleStructExports_;
```

In `TypeChecker::check()`, clear it:

```cpp
    moduleStructExports_.clear();
```

At the end of `checkExport()`, replace the struct branch with:

```cpp
    } else if (const auto* structure = dynamic_cast<const StructDeclStmt*>(statement.declaration.get())) {
        exportedName = structure->name.lexeme;
        const StructTypeDecl* structType = findStructType(exportedName);
        if (!structType) {
            throw TypeError(structure->name, "unknown struct type `" + exportedName + "`");
        }
        moduleStructExports_[moduleStack_.back()].emplace(exportedName, *structType);
        return;
```

In `checkImport()`, after value exports, add:

```cpp
    const auto structExports = moduleStructExports_.find(imported->moduleId);
    if (structExports != moduleStructExports_.end()) {
        for (const auto& entry : structExports->second) {
            if (structTypes_.find(entry.first) != structTypes_.end()) {
                Token name{TokenType::Identifier, entry.first, statement.keyword.line, statement.keyword.column};
                throw TypeError(name, "duplicate struct `" + entry.first + "`");
            }
            structTypes_.emplace(entry.first, entry.second);
        }
    }
```

- [ ] **Step 9: Route wrapper nodes in `checkStatement()`**

At the top of `TypeChecker::checkStatement()`, before `StructDeclStmt`, add:

```cpp
    if (const auto* module = dynamic_cast<const ModuleStmt*>(&statement)) {
        checkModule(*module);
        return;
    }

    if (const auto* import = dynamic_cast<const ImportStmt*>(&statement)) {
        checkImport(*import);
        return;
    }

    if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&statement)) {
        checkExport(*exportStmt);
        return;
    }
```

- [ ] **Step 10: Build and run targeted type tests**

Run:

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: module export success/type-error behavior is closer, but IR may still fail on wrappers until Task 6. If type-error first lines differ only by location from combined-source behavior, update the two `.err` fixtures after checking the new output is sensible.

- [ ] **Step 11: Commit type-checker module support**

```sh
git add include/TypeChecker.hpp src/TypeChecker.cpp tests/golden/type_errors/module_private_name.err tests/golden/type_errors/module_duplicate_export_main.err
git commit -m "feat: type check module exports"
```

---

### Task 6: Lower module wrappers through IR without runtime module semantics

**Files:**
- Modify: `src/IRCompiler.cpp`

- [ ] **Step 1: Skip module/import/export wrappers in IR compiler**

At the top of `IRCompiler::compileStatement()`, before `StructDeclStmt`, add:

```cpp
    if (const auto* module = dynamic_cast<const ModuleStmt*>(&statement)) {
        for (const auto& child : module->statements) {
            compileStatement(*child);
        }
        return;
    }

    if (dynamic_cast<const ImportStmt*>(&statement)) {
        return;
    }

    if (const auto* exportStmt = dynamic_cast<const ExportStmt*>(&statement)) {
        compileStatement(*exportStmt->declaration);
        return;
    }
```

- [ ] **Step 2: Build and run module success fixtures**

Run:

```sh
cmake --build build
python3 tests/run_golden_tests.py ./build/compiler_design
```

Expected: module export success fixtures pass or show only AST/IR golden missing failures. If `run.out` passes but missing `ast.out`/`ir.out` is reported, keep success fixtures with only `run.out`.

- [ ] **Step 3: Commit IR wrapper lowering**

```sh
git add src/IRCompiler.cpp
git commit -m "feat: lower module wrappers"
```

---

### Task 7: Add bytecode/Rust VM parity fixture

**Files:**
- Create: `tests/bytecode_artifacts/module_exports/input.cd`
- Create: `tests/bytecode_artifacts/module_exports/lib.cd`
- Create: `tests/bytecode_artifacts/module_exports/run.out`
- Modify: `tests/run_rust_vm_tests.py`

- [ ] **Step 1: Create bytecode artifact fixture**

Create `tests/bytecode_artifacts/module_exports/input.cd`:

```cd
import "./lib.cd";
print answer();
```

Create `tests/bytecode_artifacts/module_exports/lib.cd`:

```cd
let base = 39;
fun add3(x) { return x + 3; }
export fun answer() { return add3(base); }
```

Create `tests/bytecode_artifacts/module_exports/run.out`:

```text
42
```

- [ ] **Step 2: Add fixture to Rust VM golden allowlist**

In `tests/run_rust_vm_tests.py`, add `
"module_exports",` alphabetically near `multi_file_functions`:

```python
            "module_exports",
```

- [ ] **Step 3: Refresh bytecode artifact expected file**

Run:

```sh
python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs --update
```

Expected: `tests/bytecode_artifacts/module_exports/expected.cdbc` is created. Inspect it and verify it contains normal functions/vars only, no module opcode.

- [ ] **Step 4: Run Rust VM parity for the new case**

Run:

```sh
python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --cases module_exports
```

Expected:

```text
rust vm tests: 1 passed, 0 failed
```

- [ ] **Step 5: Commit parity fixture**

```sh
git add tests/bytecode_artifacts/module_exports tests/run_rust_vm_tests.py
git commit -m "test: cover module exports in bytecode parity"
```

---

### Task 8: Update docs and roadmap

**Files:**
- Modify: `docs/language-grammar.ebnf`
- Modify: `README.md`
- Modify: `docs/roadmap.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Update grammar**

In `docs/language-grammar.ebnf`, change declaration grammar to:

```ebnf
declaration = importDecl
            | exportDecl
            | structDecl
            | funDecl
            | letDecl
            | statement ;

exportDecl = "export", ( structDecl | funDecl | letDecl ) ;
```

Keep `importDecl = "import", string, ";" ;` unchanged.

- [ ] **Step 2: Update README source imports section**

In `README.md`, replace the limitation paragraph under `### Source imports` with text equivalent to:

```md
Imported files have module-private top-level scope. Only declarations marked with
`export` are introduced into the importing file's top-level scope.

This phase supports `export let`, `export fun`, and `export struct`. It does not
add namespaces, `import ... as name`, re-export syntax, package search paths,
separate compilation, or imports from stdin. `import` inside strings or `//`
comments is ignored by the loader.
```

Also add this example:

```cd
// lib.cd
let hidden = 1;
export fun visible() { return hidden + 1; }

// main.cd
import "./lib.cd";
print visible();
```

- [ ] **Step 3: Update roadmap Phase 14**

In `docs/roadmap.md`, update Phase 14 status to include:

```md
Phase 14C is implemented: imported files have module-private top-level scope, and `export let`, `export fun`, and `export struct` expose selected declarations to importers while keeping private helpers hidden.
```

Also remove `exports` from the remaining-future list and keep namespaces/package search/separate compilation/file-aware diagnostics as future work.

- [ ] **Step 4: Update AGENTS current semantics**

In `AGENTS.md`, replace the import limitation sentence with:

```md
Top-level `import "path";` directives load dependency files relative to the importing file. Imported files have module-private top-level scope; `export let`, `export fun`, and `export struct` expose selected declarations to importers, while private declarations remain visible only inside the imported file. Duplicate canonical imports are no-ops, imports from stdin are rejected, and this phase has no namespace import syntax, re-exports, package search paths, separate compilation, or file-aware diagnostics.
```

- [ ] **Step 5: Commit docs**

```sh
git add docs/language-grammar.ebnf README.md docs/roadmap.md AGENTS.md
git commit -m "docs: document module exports"
```

---

### Task 9: Full verification and cleanup

**Files:**
- Generated files only if tests refresh goldens.

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

Expected: only intentional files modified. If verification refreshed any expected artifacts, inspect them before committing.

- [ ] **Step 10: Final commit if needed**

If Task 9 produced intentional updates, commit them:

```sh
git add <intentional-files>
git commit -m "test: refresh module export artifacts"
```

---

## Self-Review Notes

- Spec coverage: Tasks cover `export let/fun/struct`, module-private imported scope, duplicate import/name errors, structured source loading, parser/AST changes, type checker export tables, IR wrapper lowering, bytecode/Rust VM parity, docs, and full verification.
- Non-goals preserved: no namespace import, re-export, package paths, separate compilation, file-aware diagnostics, or runtime module opcode.
- Placeholder scan: no placeholder markers or unspecified test commands remain.
- Known risk to watch during execution: `SourceManager::loadFileUnit()` must map canonical imported paths back to unit ids consistently. If duplicate direct CLI entries produce duplicate entry ids, keep duplicate canonical loads a no-op and do not duplicate statements.
