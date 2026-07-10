#!/usr/bin/env python3

import argparse
import difflib
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CheckResult:
    name: str
    passed: bool
    message: str = ""


SUCCESS_CHECKS = (
    ("default(ast)", (), "ast.out"),
    ("--ir", ("--ir",), "ir.out"),
    ("--bytecode", ("--bytecode",), "bytecode.out"),
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def compiler_inputs(case_dir: Path) -> list[Path]:
    args_path = case_dir / "args.txt"
    if args_path.is_file():
        entries = read_text(args_path).split()
        return [case_dir / entry for entry in entries]
    return [case_dir / "input.cd"]


def unified_diff(expected: str, actual: str, fromfile: str, tofile: str) -> str:
    return "".join(
        difflib.unified_diff(
            expected.splitlines(keepends=True),
            actual.splitlines(keepends=True),
            fromfile=fromfile,
            tofile=tofile,
        )
    )


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


def run_compiler(compiler: Path, args: tuple[str, ...], source_or_sources) -> subprocess.CompletedProcess[str]:
    if isinstance(source_or_sources, list):
        sources = source_or_sources
    else:
        sources = [source_or_sources]
    command = [str(compiler), *args, *(str(source) for source in sources)]
    return subprocess.run(command, text=True, capture_output=True, check=False)


def discover_success_cases(golden_dir: Path) -> list[Path]:
    return sorted(
        case_dir
        for case_dir in golden_dir.iterdir()
        if case_dir.is_dir()
        and case_dir.name not in {"runtime_errors", "parse_errors", "type_errors", "import_errors"}
        and ((case_dir / "input.cd").is_file() or (case_dir / "args.txt").is_file())
    )


def discover_parse_error_cases(golden_dir: Path) -> list[Path]:
    parse_dir = golden_dir / "parse_errors"
    if not parse_dir.is_dir():
        return []
    return sorted(parse_dir.glob("*.cd"))


def discover_type_error_cases(golden_dir: Path) -> list[Path]:
    type_dir = golden_dir / "type_errors"
    if not type_dir.is_dir():
        return []
    return sorted(type_dir.glob("*.cd"))


def discover_import_error_cases(golden_dir: Path) -> list[Path]:
    import_dir = golden_dir / "import_errors"
    if not import_dir.is_dir():
        return []
    return sorted(import_dir.glob("*.cd"))


def check_success_case(
    compiler: Path,
    case_dir: Path,
    update: bool,
    update_missing: bool = False,
) -> list[CheckResult]:
    sources = compiler_inputs(case_dir)
    results: list[CheckResult] = []
    compiler_golden_names = tuple(golden_name for _, _, golden_name in SUCCESS_CHECKS)
    fixture_marker_names = (*compiler_golden_names, "run.out")

    if not any((case_dir / golden_name).exists() for golden_name in fixture_marker_names):
        update_hint = " Pass --update-missing with --update to create them." if update else ""
        return [
            CheckResult(
                case_dir.name,
                False,
                (
                    f"FAIL {case_dir.name} has no expected files; "
                    f"expected at least one of: {', '.join(fixture_marker_names)}"
                    f"{update_hint}"
                ),
            )
        ]

    if not any((case_dir / golden_name).exists() for golden_name in compiler_golden_names):
        return []

    for display_name, args, golden_name in SUCCESS_CHECKS:
        golden_path = case_dir / golden_name
        if update:
            if not update_missing and not golden_path.exists():
                continue
        elif not golden_path.exists():
            continue

        completed = run_compiler(compiler, args, sources)
        check_name = f"{case_dir.name} {display_name}"

        if completed.returncode != 0:
            results.append(
                CheckResult(
                    check_name,
                    False,
                    f"FAIL {check_name} exited with {completed.returncode}\n\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}",
                )
            )
            continue

        if completed.stderr:
            results.append(
                CheckResult(
                    check_name,
                    False,
                    (
                        f"FAIL {check_name} produced unexpected stderr with exit code 0\n\n"
                        f"STDERR:\n{completed.stderr}"
                    ),
                )
            )
            continue

        if update:
            write_text(golden_path, completed.stdout)
            results.append(CheckResult(check_name, True))
            continue

        expected = read_text(golden_path)
        actual = completed.stdout
        if actual != expected:
            diff = unified_diff(expected, actual, "expected", "actual")
            results.append(CheckResult(check_name, False, f"FAIL {check_name} stdout mismatch\n\n{diff}"))
        else:
            results.append(CheckResult(check_name, True))

    return results


def unexpected_parse_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for parse error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )


def unexpected_type_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for type error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )


def unexpected_import_stdout_result(case_name: str, stdout: str) -> CheckResult:
    return CheckResult(
        case_name,
        False,
        (
            f"FAIL {case_name} produced unexpected stdout for import error\n\n"
            f"STDOUT:\n{stdout}"
        ),
    )


def check_parse_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"parse_errors/{source.stem} default(ast)"

    completed = run_compiler(compiler, (), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_parse_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_parse_stdout_result(case_name, completed.stdout))

    if not err_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected stderr file: {err_path}"))
    else:
        expected_err = read_text(err_path)
        actual_err = completed.stderr
        if not located_diagnostic_stderr_matches(expected_err, actual_err):
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



def check_type_error_case(compiler: Path, source: Path, update: bool) -> list[CheckResult]:
    stem = source.with_suffix("")
    err_path = stem.with_suffix(".err")
    exit_path = stem.with_suffix(".exit")
    case_name = f"type_errors/{source.stem} default(ast)"

    completed = run_compiler(compiler, (), source)

    if update:
        write_text(err_path, completed.stderr)
        write_text(exit_path, f"{completed.returncode}\n")
        if completed.stdout:
            return [unexpected_type_stdout_result(case_name, completed.stdout)]
        return [CheckResult(case_name, True)]

    results: list[CheckResult] = []

    if completed.stdout:
        results.append(unexpected_type_stdout_result(case_name, completed.stdout))

    if not err_path.exists():
        results.append(CheckResult(case_name, False, f"FAIL {case_name} missing expected stderr file: {err_path}"))
    else:
        expected_err = read_text(err_path)
        actual_err = completed.stderr
        if not located_diagnostic_stderr_matches(expected_err, actual_err):
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

def case_matches(case_name: str, case_filters: tuple[str, ...]) -> bool:
    return not case_filters or any(case_filter in case_name for case_filter in case_filters)


def run_all(
    compiler: Path,
    golden_dir: Path,
    update: bool,
    update_missing: bool = False,
    case_filters: tuple[str, ...] = (),
) -> list[CheckResult]:
    results: list[CheckResult] = []

    for case_dir in discover_success_cases(golden_dir):
        if case_matches(case_dir.name, case_filters):
            results.extend(check_success_case(compiler, case_dir, update, update_missing))

    for source in discover_parse_error_cases(golden_dir):
        if case_matches(f"parse_errors/{source.stem}", case_filters):
            results.extend(check_parse_error_case(compiler, source, update))

    for source in discover_type_error_cases(golden_dir):
        if case_matches(f"type_errors/{source.stem}", case_filters):
            results.extend(check_type_error_case(compiler, source, update))

    for source in discover_import_error_cases(golden_dir):
        if case_matches(f"import_errors/{source.stem}", case_filters):
            results.extend(check_import_error_case(compiler, source, update))

    if not results:
        results.append(CheckResult("golden", False, "FAIL golden tests found no golden test checks/results"))

    return results


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run compiler CLI golden tests.")
    parser.add_argument("compiler", type=Path, help="Path to compiler_design executable")
    parser.add_argument("--update", action="store_true", help="Rewrite golden files from current compiler output")
    parser.add_argument(
        "--update-missing",
        action="store_true",
        help="With --update, also create missing expected files for success fixtures",
    )
    parser.add_argument(
        "--case",
        dest="case_filters",
        action="append",
        default=[],
        help="Only run/update cases whose fixture name contains this substring; may be repeated",
    )
    args = parser.parse_args()
    if args.update_missing and not args.update:
        parser.error("--update-missing requires --update")
    return args


def main() -> int:
    args = parse_args()
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        print(f"compiler not found: {compiler}", file=sys.stderr)
        return 64

    tests_dir = Path(__file__).resolve().parent
    golden_dir = tests_dir / "golden"
    if not golden_dir.is_dir():
        print(f"golden directory not found: {golden_dir}", file=sys.stderr)
        return 64

    results = run_all(
        compiler,
        golden_dir,
        args.update,
        args.update_missing,
        tuple(args.case_filters),
    )
    failed = [result for result in results if not result.passed]

    for failure in failed:
        print(failure.message, file=sys.stderr)

    passed_count = len(results) - len(failed)
    print(f"golden tests: {passed_count} passed, {len(failed)} failed")

    return 1 if failed else 0

if __name__ == "__main__":
    raise SystemExit(main())
