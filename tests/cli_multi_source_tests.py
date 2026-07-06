#!/usr/bin/env python3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class CliMultiSourceTests(unittest.TestCase):
    def setUp(self) -> None:
        if len(sys.argv) != 2:
            self.fail("usage: cli_multi_source_tests.py <compiler>")
        self.compiler = Path(sys.argv[1]).resolve()
        if not self.compiler.is_file():
            self.fail(f"compiler not found: {self.compiler}")

    def run_compiler(self, *args: str, input_text: str | None = None) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.compiler), *args],
            input=input_text,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_run_accepts_multiple_input_files_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            main = root / "main.cd"
            lib.write_text("fun add(a, b) { return a + b; }\n", encoding="utf-8")
            main.write_text("print add(1, 2);\n", encoding="utf-8")

            completed = self.run_compiler("--run", str(lib), str(main))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stderr, "")
            self.assertEqual(completed.stdout, "3\n")

    def test_emit_bytecode_accepts_multiple_input_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            lib = root / "lib.cd"
            main = root / "main.cd"
            artifact = root / "program.cdbc"
            lib.write_text("fun add(a, b) { return a + b; }\n", encoding="utf-8")
            main.write_text("print add(2, 3);\n", encoding="utf-8")

            completed = self.run_compiler("--emit-bytecode", str(artifact), str(lib), str(main))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, "")
            self.assertTrue(artifact.is_file())
            self.assertIn("add", artifact.read_text(encoding="utf-8"))

    def test_missing_second_input_file_reports_path(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            existing = root / "existing.cd"
            missing = root / "missing.cd"
            existing.write_text("print 1;\n", encoding="utf-8")

            completed = self.run_compiler("--run", str(existing), str(missing))

            self.assertEqual(completed.returncode, 1)
            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, f"failed to open input file: {missing}\n")

    def test_stdin_still_works_when_no_input_files_are_given(self) -> None:
        completed = self.run_compiler("--run", input_text="print 7;\n")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stderr, "")
        self.assertEqual(completed.stdout, "7\n")

    def test_emit_bytecode_still_requires_at_least_one_input_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            artifact = Path(temp_dir) / "program.cdbc"

            completed = self.run_compiler("--emit-bytecode", str(artifact), input_text="print 1;\n")

            self.assertEqual(completed.returncode, 64)
            self.assertEqual(completed.stdout, "")
            self.assertFalse(artifact.exists())


    def test_import_from_stdin_is_rejected(self) -> None:
        completed = self.run_compiler(input_text='import "./lib.cd";\n')

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")

    def test_import_path_resolves_relative_to_importing_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            nested = root / "nested"
            nested.mkdir()
            (root / "input.cd").write_text('import "./nested/lib.cd";\nprint value;\n', encoding="utf-8")
            (nested / "lib.cd").write_text('import "./inner.cd";\n', encoding="utf-8")
            (nested / "inner.cd").write_text('let value = "relative";\n', encoding="utf-8")

            completed = self.run_compiler("--run", str(root / "input.cd"))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, "relative\n")
            self.assertEqual(completed.stderr, "")

    def test_import_text_inside_string_and_comment_is_ignored_by_loader(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "input.cd").write_text(
                '// import "./missing_from_comment.cd";\n'
                'print "import ./missing_from_string.cd;";\n',
                encoding="utf-8",
            )

            completed = self.run_compiler("--run", str(root / "input.cd"))

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(completed.stdout, 'import ./missing_from_string.cd;\n')
            self.assertEqual(completed.stderr, "")


    def test_lex_error_in_stdin_prints_source_snippet(self) -> None:
        completed = self.run_compiler(input_text="print @;\n")

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(
            completed.stderr,
            "Lex error at 1:7: unexpected character `@`\n"
            "  print @;\n"
            "        ^\n",
        )

    def test_import_error_stays_one_line_without_source_snippet(self) -> None:
        completed = self.run_compiler(input_text='import "./lib.cd";\n')

        self.assertEqual(completed.returncode, 1)
        self.assertEqual(completed.stdout, "")
        self.assertEqual(completed.stderr, "Import error: import is not supported from stdin\n")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
