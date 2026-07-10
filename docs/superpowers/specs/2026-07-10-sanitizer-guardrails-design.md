# Sanitizer Guardrails Design

## Goal

Complete the remaining M0 front-end stabilization item by adding opt-in C++ sanitizer build support and a separate GitHub Actions sanitizer job. The slice must not change compiler behavior, language semantics, diagnostics, bytecode output, or golden expectations.

## Scope

In scope:

- Add a CMake option named `COMPILER_DESIGN_ENABLE_SANITIZERS`.
- When enabled on GCC or Clang, apply AddressSanitizer and UndefinedBehaviorSanitizer compile/link flags to repository C++ targets.
- Apply sanitizer flags to:
  - `compiler_design`
  - `frontend_session_tests`
  - `flow_facts_tests`
  - `module_symbols_tests`
- Add a GitHub Actions job separate from the existing `verify` job.
- The sanitizer job should configure with both warnings and sanitizers enabled, build all CMake targets, and run CTest.
- Add focused configuration/workflow tests so the new guardrail is covered by existing CTest.

Out of scope:

- Running the standalone Python golden/Rust VM/cargo commands a second time in the sanitizer job. CTest already invokes the compiler golden, bytecode artifact, Rust VM golden, and C++ unit tests; the main `verify` job remains responsible for the explicit full command list.
- MSVC sanitizer support. If the option is enabled under unsupported compilers, it should simply avoid adding unsupported sanitizer flags.
- Making sanitizers enabled by default.
- Treating existing non-sanitizer compiler warnings as errors.
- Fixing unrelated sanitizer findings unless they are required to make the sanitizer job pass.

## Proposed Approach

Extend `CMakeLists.txt` with a sanitizer option and helper function similar to the existing warnings helper. The helper will add both compile and link options to each C++ target when all of these are true:

- `COMPILER_DESIGN_ENABLE_SANITIZERS` is `ON`;
- the compiler is GCC or Clang;
- the build is not using MSVC.

Use this flag set for GCC/Clang:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

Apply these via `target_compile_options` and `target_link_options` so target-level behavior remains explicit and does not leak to external projects.

Extend `.github/workflows/ci.yml` with a second job named `sanitizers`. This job should run on `ubuntu-latest`, check out the repo, install the Rust toolchain because CTest includes Rust VM tests, configure CMake with:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
```

then run:

```sh
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Keep the existing `verify` job unchanged except for any workflow-test expectations.

## Test Strategy

Add or extend Python infrastructure tests:

- `tests/cmake_config_tests.py` should configure a temporary build with `-DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON` and assert `COMPILER_DESIGN_ENABLE_SANITIZERS:BOOL=ON` appears in `CMakeCache.txt`.
- `tests/ci_workflow_tests.py` should assert the workflow contains the sanitizer job name, the sanitizer configure command, the sanitizer build command, and the sanitizer CTest command.

Then verify locally:

```sh
cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Finally run the standard full verification suite from AGENTS.md using the normal build directory.

## Failure Handling

If sanitizer CTest finds an actual memory or undefined-behavior issue, fix the underlying C++ defect in the same slice only if the root cause is clear and tightly scoped. If the issue is broad or architectural, stop and report the finding rather than masking it with suppressions.

Do not add sanitizer suppressions in this slice unless there is a proven external false positive. Do not weaken normal golden assertions to make sanitizer runs pass.

## Documentation Impact

Update `README.md` near the existing build options to show the sanitizer configure command. Keep it clear that sanitizers are opt-in local/CI guardrails, not required for ordinary builds.

## Self-Review

- Placeholder scan: no placeholders or TODOs remain.
- Internal consistency: the option name, CMake behavior, CI job, and tests all use `COMPILER_DESIGN_ENABLE_SANITIZERS`.
- Scope check: this is a focused M0 guardrail slice; sanitizer bug fixing is limited to findings required for the job to pass.
- Ambiguity check: supported compilers, target list, CI commands, and documentation updates are explicit.
