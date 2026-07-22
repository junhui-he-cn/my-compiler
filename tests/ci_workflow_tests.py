#!/usr/bin/env python3

import sys
from pathlib import Path


REQUIRED_SNIPPETS = (
    "name: CI",
    "push:",
    "pull_request:",
    "branches: [master]",
    "uses: actions/checkout@v4",
    "uses: dtolnay/rust-toolchain@stable",
    "cmake -S . -B build -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON",
    "cmake --build build",
    "ctest --test-dir build --output-on-failure",
    "python3 tests/run_golden_tests.py ./build/compiler_design",
    "python3 tests/run_boundary_tests.py ./build/compiler_design",
    "python3 tests/run_malformed_tests.py ./build/compiler_design vm-rs --report build/malformed-report.json",
    "python3 tests/run_golden_tests_selftest.py",
    "python3 tests/bytecode_artifact_tests.py ./build/compiler_design vm-rs",
    "python3 tests/run_rust_vm_tests.py ./build/compiler_design vm-rs --goldens",
    "cargo test --manifest-path vm-rs/Cargo.toml",
    "python3 tests/run_verification.py ./build/compiler_design vm-rs --report build/verification-report.json",
    "uses: actions/upload-artifact@v4",
    "name: verification-report",
    "sanitizers:",
    "name: Sanitizers",
    "cmake -S . -B build-sanitize -DCOMPILER_DESIGN_ENABLE_WARNINGS=ON -DCOMPILER_DESIGN_ENABLE_SANITIZERS=ON",
    "cmake --build build-sanitize",
    "ctest --test-dir build-sanitize --output-on-failure",
)


def fail(message: str) -> None:
    print(f"FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: ci_workflow_tests.py <repo-root>")

    repo_root = Path(sys.argv[1]).resolve()
    workflow_path = repo_root / ".github" / "workflows" / "ci.yml"
    if not workflow_path.is_file():
        fail(f"missing workflow file: {workflow_path}")

    workflow = workflow_path.read_text(encoding="utf-8")
    missing = [snippet for snippet in REQUIRED_SNIPPETS if snippet not in workflow]
    if missing:
        fail("workflow missing required snippets: " + ", ".join(repr(item) for item in missing))


if __name__ == "__main__":
    main()
