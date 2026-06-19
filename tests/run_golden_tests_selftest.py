#!/usr/bin/env python3

import tempfile
import textwrap
import unittest
from pathlib import Path

import run_golden_tests


class GoldenRunnerQualityTests(unittest.TestCase):
    def make_fake_compiler(
        self,
        directory: Path,
        stdout: str = "",
        stderr: str = "",
        returncode: int = 0,
    ) -> Path:
        compiler = directory / "fake_compiler.py"
        compiler.write_text(
            textwrap.dedent(
                f"""\
                #!/usr/bin/env python3
                import sys

                sys.stdout.write({stdout!r})
                sys.stderr.write({stderr!r})
                raise SystemExit({returncode})
                """
            ),
            encoding="utf-8",
        )
        compiler.chmod(compiler.stat().st_mode | 0o111)
        return compiler

    def test_non_update_success_case_without_expected_files_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "empty_success_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("1;\n", encoding="utf-8")
            compiler = self.make_fake_compiler(root)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("has no expected files", results[0].message)

    def test_non_update_empty_suite_fails_instead_of_passing_zero_checks(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            golden_dir.mkdir()
            compiler = self.make_fake_compiler(root)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("no golden test checks", results[0].message)

    def test_success_case_with_unexpected_stderr_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "stderr_success_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("1;\n", encoding="utf-8")
            (case_dir / "ast.out").write_text("ok\n", encoding="utf-8")
            compiler = self.make_fake_compiler(root, stdout="ok\n", stderr="warning\n")

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("unexpected stderr", results[0].message)
        self.assertIn("warning", results[0].message)

    def test_default_ast_check_name_does_not_look_like_missing_cli_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "default_ast_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("1;\n", encoding="utf-8")
            (case_dir / "ast.out").write_text("expected\n", encoding="utf-8")
            compiler = self.make_fake_compiler(root, stdout="actual\n")

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("default_ast_case default(ast)", results[0].message)
        self.assertNotIn("--ast", results[0].message)

    def test_runtime_error_case_with_unexpected_stdout_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "stdout_leak.cd").write_text("1 / 0;\n", encoding="utf-8")
            (runtime_dir / "stdout_leak.run.err").write_text("runtime error\n", encoding="utf-8")
            (runtime_dir / "stdout_leak.exit").write_text("70\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="runtime error\n",
                returncode=70,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
