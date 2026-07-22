#!/usr/bin/env python3

"""Shared boundary names, canonicalization, and first-failure reporting."""

from __future__ import annotations

import difflib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


TESTS_DIR = Path(__file__).resolve().parent
DEFAULT_ALLOWLIST = TESTS_DIR / "boundary_allowlist.json"

# This order is the dependency order of deterministic compiler/runtime
# boundaries.  ``verification`` is reserved for named test suites that do not
# expose a compiler pipeline boundary of their own.
BOUNDARY_ORDER = (
    "tokens",
    "ast",
    "parse_diagnostic",
    "import_diagnostic",
    "semantic",
    "semantic_diagnostic",
    "ir",
    "bytecode",
    "module_interface",
    "cdbc",
    "rust_decode",
    "vm_output",
    "verification",
)


@dataclass(frozen=True)
class BoundaryComparison:
    boundary: str
    matches: bool
    expected: str
    actual: str
    diff: str = ""


def load_allowlist(path: Path = DEFAULT_ALLOWLIST) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"boundary allowlist root must be an object: {path}")
    if value.get("schema_version") != 1:
        raise ValueError(f"unsupported boundary allowlist schema: {path}")
    rules = value.get("rules")
    if not isinstance(rules, list):
        raise ValueError(f"boundary allowlist rules must be a list: {path}")
    for rule in rules:
        if not isinstance(rule, dict) or not isinstance(rule.get("pattern"), str) or not isinstance(rule.get("replacement"), str):
            raise ValueError(f"invalid boundary allowlist rule: {rule!r}")
    return value


def canonicalize(text: str, allowlist: Optional[dict[str, object]] = None) -> str:
    import re

    selected = allowlist if allowlist is not None else load_allowlist()
    result = text
    for rule in selected["rules"]:
        result = re.sub(str(rule["pattern"]), str(rule["replacement"]), result)
    return result


def compare_text(
    boundary: str,
    expected: str,
    actual: str,
    allowlist: Optional[dict[str, object]] = None,
) -> BoundaryComparison:
    canonical_expected = canonicalize(expected, allowlist)
    canonical_actual = canonicalize(actual, allowlist)
    if canonical_expected == canonical_actual:
        return BoundaryComparison(boundary, True, canonical_expected, canonical_actual)
    diff = "".join(
        difflib.unified_diff(
            canonical_expected.splitlines(keepends=True),
            canonical_actual.splitlines(keepends=True),
            fromfile="expected",
            tofile="actual",
        )
    )
    return BoundaryComparison(boundary, False, canonical_expected, canonical_actual, diff)


def boundary_rank(boundary: str) -> int:
    try:
        return BOUNDARY_ORDER.index(boundary)
    except ValueError:
        return len(BOUNDARY_ORDER)


def failure_boundary(case: dict[str, object]) -> str:
    terminal = case.get("terminal_boundary")
    if isinstance(terminal, str) and terminal:
        return terminal
    sequence = case.get("boundary_sequence")
    if isinstance(sequence, list) and sequence:
        return str(sequence[-1])
    return "verification"


def first_failure(cases: Iterable[dict[str, object]]) -> Optional[dict[str, str]]:
    failures = []
    for case in cases:
        if case.get("passed", True):
            continue
        boundary = str(case.get("failure_boundary") or failure_boundary(case))
        failures.append((boundary_rank(boundary), str(case["case_id"]), boundary))
    if not failures:
        return None
    _, case_id, boundary = min(failures)
    return {"case_id": case_id, "boundary": boundary}


def failure_counts(cases: Iterable[dict[str, object]]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for case in cases:
        if case.get("passed", True):
            continue
        boundary = str(case.get("failure_boundary") or failure_boundary(case))
        counts[boundary] = counts.get(boundary, 0) + 1
    return dict(sorted(counts.items(), key=lambda item: (boundary_rank(item[0]), item[0])))
