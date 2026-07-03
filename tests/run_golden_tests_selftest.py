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


    def test_runtime_error_case_without_run_err_fails_for_run_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "missing_run_err.cd").write_text("1 / 0;\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="runtime error\n",
                returncode=70,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 2)
        self.assertTrue(all(not result.passed for result in results))
        combined_messages = "\n".join(result.message for result in results)
        self.assertIn("missing expected stderr file", combined_messages)
        self.assertIn("missing expected exit file", combined_messages)
        self.assertIn("runtime_errors/missing_run_err --run", combined_messages)

    def test_runtime_error_case_without_run_bytecode_err_skips_run_bytecode_without_invoking_compiler(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "run_only_error.cd").write_text("1 / 0;\n", encoding="utf-8")
            (runtime_dir / "run_only_error.run.err").write_text("runtime error\n", encoding="utf-8")
            (runtime_dir / "run_only_error.exit").write_text("70\n", encoding="utf-8")
            invocation_log = root / "invocations.txt"
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    f"""\
                    #!/usr/bin/env python3
                    import pathlib
                    import sys

                    pathlib.Path({str(invocation_log)!r}).write_text(" ".join(sys.argv[1:]) + "\\n")
                    if "--run-bytecode" in sys.argv:
                        sys.stderr.write("unexpected --run-bytecode invocation\\n")
                        raise SystemExit(99)
                    sys.stderr.write("runtime error\\n")
                    raise SystemExit(70)
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)
            invocations = invocation_log.read_text(encoding="utf-8")

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "runtime_errors/run_only_error --run")
        self.assertNotIn("--run-bytecode", invocations)


    def test_runtime_error_case_with_orphan_run_bytecode_exit_fails_missing_stderr(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "orphan_bytecode_exit.cd").write_text("1 / 0;\n", encoding="utf-8")
            (runtime_dir / "orphan_bytecode_exit.run.err").write_text("runtime error\n", encoding="utf-8")
            (runtime_dir / "orphan_bytecode_exit.exit").write_text("70\n", encoding="utf-8")
            (runtime_dir / "orphan_bytecode_exit.run_bytecode.exit").write_text("70\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="runtime error\n",
                returncode=70,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 2)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "runtime_errors/orphan_bytecode_exit --run")
        self.assertFalse(results[1].passed)
        self.assertIn("runtime_errors/orphan_bytecode_exit --run-bytecode", results[1].message)
        self.assertIn("missing expected stderr file", results[1].message)


    def test_parse_error_case_checks_default_mode_stderr_exit_and_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            parse_dir = golden_dir / "parse_errors"
            parse_dir.mkdir(parents=True)
            (parse_dir / "bad_assignment.cd").write_text("(x + 1) = 2;\n", encoding="utf-8")
            (parse_dir / "bad_assignment.err").write_text("parse error\n", encoding="utf-8")
            (parse_dir / "bad_assignment.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="parse error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("parse_errors/bad_assignment default(ast)", results[0].message)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)

    def test_parse_error_input_cd_is_not_discovered_as_success_case(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            parse_dir = golden_dir / "parse_errors"
            parse_dir.mkdir(parents=True)
            (parse_dir / "input.cd").write_text("(x + 1) = 2;\n", encoding="utf-8")
            (parse_dir / "input.err").write_text("parse error\n", encoding="utf-8")
            (parse_dir / "input.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="parse error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "parse_errors/input default(ast)")


    def test_type_error_case_checks_default_mode_stderr_exit_and_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            type_dir = golden_dir / "type_errors"
            type_dir.mkdir(parents=True)
            (type_dir / "bad_type.cd").write_text('let x: number = "bad";\n', encoding="utf-8")
            (type_dir / "bad_type.err").write_text("type error\n", encoding="utf-8")
            (type_dir / "bad_type.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stdout="unexpected output\n",
                stderr="type error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertFalse(results[0].passed)
        self.assertIn("type_errors/bad_type default(ast)", results[0].message)
        self.assertIn("unexpected stdout", results[0].message)
        self.assertIn("unexpected output", results[0].message)

    def test_type_error_input_cd_is_not_discovered_as_success_case(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            type_dir = golden_dir / "type_errors"
            type_dir.mkdir(parents=True)
            (type_dir / "input.cd").write_text('let x: number = "bad";\n', encoding="utf-8")
            (type_dir / "input.err").write_text("type error\n", encoding="utf-8")
            (type_dir / "input.exit").write_text("1\n", encoding="utf-8")
            compiler = self.make_fake_compiler(
                root,
                stderr="type error\n",
                returncode=1,
            )

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "type_errors/input default(ast)")


    def test_success_case_with_bytecode_expected_file_runs_bytecode_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "bytecode_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "bytecode.out").write_text("bytecode output\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--bytecode" not in sys.argv:
                        sys.stderr.write("missing --bytecode\\n")
                        raise SystemExit(1)
                    sys.stdout.write("bytecode output\\n")
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "bytecode_case --bytecode")

    def test_success_case_with_run_bytecode_expected_file_runs_run_bytecode_flag(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            case_dir = golden_dir / "run_bytecode_case"
            case_dir.mkdir(parents=True)
            (case_dir / "input.cd").write_text("print 1;\n", encoding="utf-8")
            (case_dir / "run_bytecode.out").write_text("1\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--run-bytecode" not in sys.argv:
                        sys.stderr.write("missing --run-bytecode\\n")
                        raise SystemExit(1)
                    sys.stdout.write("1\\n")
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 1)
        self.assertTrue(results[0].passed)
        self.assertEqual(results[0].name, "run_bytecode_case --run-bytecode")

    def test_runtime_error_case_with_run_bytecode_expected_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            golden_dir = root / "golden"
            runtime_dir = golden_dir / "runtime_errors"
            runtime_dir.mkdir(parents=True)
            (runtime_dir / "bytecode_error.cd").write_text("print 1 / 0;\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.run.err").write_text("Runtime error: division by zero\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.exit").write_text("1\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.run_bytecode.err").write_text("Runtime error: division by zero\n", encoding="utf-8")
            (runtime_dir / "bytecode_error.run_bytecode.exit").write_text("1\n", encoding="utf-8")
            compiler = root / "fake_compiler.py"
            compiler.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import sys

                    if "--run" not in sys.argv and "--run-bytecode" not in sys.argv:
                        sys.stderr.write("missing runtime flag\\n")
                        raise SystemExit(1)
                    sys.stderr.write("Runtime error: division by zero\\n")
                    raise SystemExit(1)
                    """
                ),
                encoding="utf-8",
            )
            compiler.chmod(compiler.stat().st_mode | 0o111)

            results = run_golden_tests.run_all(compiler, golden_dir, update=False)

        self.assertEqual(len(results), 2)
        self.assertTrue(all(result.passed for result in results))
        self.assertEqual(
            [result.name for result in results],
            ["runtime_errors/bytecode_error --run", "runtime_errors/bytecode_error --run-bytecode"],
        )


if __name__ == "__main__":
    raise SystemExit(unittest.main())
