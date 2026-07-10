#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path


def fail(message: str, completed: subprocess.CompletedProcess[str] | None = None) -> None:
    print(f"FAIL {message}", file=sys.stderr)
    if completed is not None:
        print("STDOUT:", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        print("STDERR:", file=sys.stderr)
        print(completed.stderr, file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: cmake_config_tests.py <repo-root>")

    repo_root = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="compiler-design-cmake-") as tmp:
        build_dir = Path(tmp) / "build"
        completed = subprocess.run(
            [
                "cmake",
                "-S",
                str(repo_root),
                "-B",
                str(build_dir),
                "-DCOMPILER_DESIGN_ENABLE_WARNINGS=ON",
            ],
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            fail("warning-enabled CMake configure exited non-zero", completed)

        cache_path = build_dir / "CMakeCache.txt"
        cache = cache_path.read_text(encoding="utf-8")
        expected = "COMPILER_DESIGN_ENABLE_WARNINGS:BOOL=ON"
        if expected not in cache:
            fail(f"expected CMake cache to contain {expected!r}")


if __name__ == "__main__":
    main()
