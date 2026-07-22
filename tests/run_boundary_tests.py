#!/usr/bin/env python3

"""Compare the checked-in lexical boundary reference corpus."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from boundary_comparison import compare_text, load_allowlist
from run_golden_tests import CheckResult


TESTS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TESTS_DIR.parent
MANIFEST_PATH = TESTS_DIR / "boundary_cases.json"
ALLOWLIST_PATH = TESTS_DIR / "boundary_allowlist.json"


def load_manifest(path: Path = MANIFEST_PATH) -> list[dict[str, object]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("schema_version") != 1:
        raise ValueError(f"invalid boundary manifest: {path}")
    cases = value.get("cases")
    if not isinstance(cases, list):
        raise ValueError(f"boundary manifest cases must be a list: {path}")
    return [case for case in cases if isinstance(case, dict)]


def run_compiler(compiler: Path, sources: list[Path]) -> subprocess.CompletedProcess[str]:
    command = [str(compiler), "--tokens", *(str(source) for source in sources)]
    return subprocess.run(command, cwd=REPO_ROOT, text=True, capture_output=True, check=False)


def check_case(
    compiler: Path,
    case: dict[str, object],
    allowlist: dict[str, object],
    update: bool,
) -> CheckResult:
    name = str(case["result_name"])
    sources = [REPO_ROOT / str(source) for source in case["sources"]]
    expected_files = [REPO_ROOT / str(path) for path in case["expected_files"]]
    if len(expected_files) != 1:
        return CheckResult(name, False, f"FAIL {name} requires exactly one expected token output")
    expected_path = expected_files[0]

    completed = run_compiler(compiler, sources)
    if completed.returncode != 0:
        return CheckResult(
            name,
            False,
            f"FAIL {name} exited with {completed.returncode}\n\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}",
        )
    if completed.stderr:
        return CheckResult(name, False, f"FAIL {name} produced unexpected stderr\n\n{completed.stderr}")

    actual = compare_text("tokens", expected_path.read_text(encoding="utf-8") if expected_path.is_file() else "", completed.stdout, allowlist)
    if update:
        expected_path.parent.mkdir(parents=True, exist_ok=True)
        expected_path.write_text(actual.actual, encoding="utf-8")
        return CheckResult(name, True)
    if not expected_path.is_file():
        return CheckResult(name, False, f"FAIL {name} missing expected output: {expected_path}")
    if not actual.matches:
        return CheckResult(name, False, f"FAIL {name} tokens boundary mismatch\n\n{actual.diff}")
    return CheckResult(name, True)


def run_all(
    compiler: Path,
    manifest_path: Path = MANIFEST_PATH,
    update: bool = False,
    case_filters: tuple[str, ...] = (),
) -> list[CheckResult]:
    manifest = load_manifest(manifest_path)
    allowlist = load_allowlist(ALLOWLIST_PATH)
    results = []
    for case in manifest:
        case_id = str(case["case_id"])
        if case_filters and not any(case_filter in case_id for case_filter in case_filters):
            continue
        results.append(check_case(compiler, case, allowlist, update))
    if not results:
        return [CheckResult("boundary_tokens", False, "FAIL no boundary token cases selected")]
    return results


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run lexical boundary reference comparisons.")
    parser.add_argument("compiler", type=Path, help="Path to compiler_design executable")
    parser.add_argument("--update", action="store_true", help="Rewrite token references from compiler output")
    parser.add_argument("--case", action="append", default=[], dest="case_filters")
    parser.add_argument("--manifest", type=Path, default=MANIFEST_PATH)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        print(f"compiler not found: {compiler}", file=sys.stderr)
        return 64
    try:
        results = run_all(compiler, args.manifest.resolve(), args.update, tuple(args.case_filters))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"boundary tests: {error}", file=sys.stderr)
        return 64
    failed = [result for result in results if not result.passed]
    for failure in failed:
        print(failure.message, file=sys.stderr)
    passed_count = len(results) - len(failed)
    print(f"boundary token tests: {passed_count} passed, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
