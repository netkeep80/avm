#!/usr/bin/env python3
"""Запустить deterministic fixtures на pinned legacy jsonRVM runtime.

Скрипт используется только как audit oracle. Он не является частью execution
path AVM и не импортирует код старого интерпретатора.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile

PINNED_COMMIT = "843b3326141e090ccd1a106ba0a4a21ce72805b7"

SUCCESS_CASES = (
    ("CASE-ARITHMETIC", "arithmetic-add.json", {"result": 2}),
    ("CASE-SEQUENCE-ORDER", "sequence-order.json", {"result": 3}),
    ("CASE-FOREACH-CONTEXT", "foreach-object-context.json", {"result": [1, 2, 3]}),
    ("CASE-BOOLEAN-BRANCH", "boolean-branch.json", {"result": 42}),
    ("CASE-PURE-RELATION-COMPOSITION", "pure-relation-composition.json", {"result": 5}),
)

MISSING_REFERENCE_MARKER = "__avm_missing_reference_oracle__"


def run_fixture(executable: pathlib.Path, fixture: pathlib.Path) -> subprocess.CompletedProcess[str]:
    entry_point = fixture.stem
    with tempfile.TemporaryDirectory(prefix=f"avm-jsonrvm-oracle-{entry_point}-") as directory:
        work = pathlib.Path(directory)

        # Pinned jsonRVM console creates file_database_t(".\\") and then
        # concatenates that string with '<entry>.json'. On Windows this is the
        # normal spelling of a relative path; on POSIX the backslash is a
        # literal filename character. Preserve the legacy spelling instead of
        # patching the old runtime for the audit job.
        legacy_fixture = work / f".\\{entry_point}.json"
        shutil.copyfile(fixture, legacy_fixture)

        return subprocess.run(
            [str(executable), entry_point],
            cwd=work,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )


def check_success_case(
    executable: pathlib.Path,
    fixture_dir: pathlib.Path,
    case_id: str,
    fixture_name: str,
    expected: object,
) -> None:
    process = run_fixture(executable, fixture_dir / fixture_name)
    if process.returncode != 0:
        raise RuntimeError(
            f"{case_id}: legacy jsonRVM завершился с кодом {process.returncode}: {process.stderr}"
        )
    try:
        actual = json.loads(process.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"{case_id}: stdout не является JSON: {process.stdout!r}") from exc
    if actual != expected:
        raise RuntimeError(f"{case_id}: ожидалось {expected!r}, получено {actual!r}")
    print(f"{case_id} OK: {json.dumps(actual, ensure_ascii=False, sort_keys=True)}")


def check_missing_reference(executable: pathlib.Path, fixture_dir: pathlib.Path) -> None:
    case_id = "CASE-MISSING-REFERENCE"
    process = run_fixture(executable, fixture_dir / "missing-reference.json")
    if process.returncode == 0:
        raise RuntimeError(f"{case_id}: missing reference неожиданно завершился успешно")
    if not process.stderr.strip():
        raise RuntimeError(f"{case_id}: legacy jsonRVM не выдал diagnostic JSON")
    try:
        json.loads(process.stderr)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"{case_id}: stderr не является JSON: {process.stderr!r}") from exc
    if MISSING_REFERENCE_MARKER not in process.stderr:
        raise RuntimeError(
            f"{case_id}: diagnostic не содержит marker {MISSING_REFERENCE_MARKER!r}"
        )
    print(f"{case_id} OK: exit={process.returncode}, diagnostic содержит marker")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=pathlib.Path, required=True)
    parser.add_argument(
        "--fixtures",
        type=pathlib.Path,
        default=pathlib.Path("compat/jsonrvm-legacy-fixtures"),
    )
    args = parser.parse_args()

    executable = args.executable.resolve()
    fixture_dir = args.fixtures.resolve()
    if not executable.is_file():
        raise SystemExit(f"Не найден legacy jsonRVM executable: {executable}")
    if not fixture_dir.is_dir():
        raise SystemExit(f"Не найден каталог fixtures: {fixture_dir}")

    print(f"Legacy oracle source commit: {PINNED_COMMIT}")
    for case in SUCCESS_CASES:
        check_success_case(executable, fixture_dir, *case)
    check_missing_reference(executable, fixture_dir)
    print("Legacy jsonRVM oracle: все deterministic cases подтверждены pinned runtime.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
