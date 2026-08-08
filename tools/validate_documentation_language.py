#!/usr/bin/env python3
"""Проверка нормативного языка проектной документации AVM.

Это намеренно лёгкий структурный guard, а не NLP-детектор языка. Он проверяет,
что каждый AVM-owned Markdown-документ содержит существенный русский prose и
что заголовки не возвращаются к полностью английской форме. Кодовые блоки и
технические identifiers не анализируются как естественный язык.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ROOT_DOCS = [ROOT / "README.md", ROOT / "analysis.md", ROOT / "plan.md"]
DOCS_DIR = ROOT / "docs"
CYRILLIC = re.compile(r"[А-Яа-яЁё]")
LATIN = re.compile(r"[A-Za-z]")
WORD = re.compile(r"[A-Za-zА-Яа-яЁё]+")

# Эти файлы принадлежат внешнему/юридическому слою и намеренно не входят в
# project-owned documentation scope. LICENSE вообще не является Markdown.
EXCLUDED_RELATIVE_PATHS = {
    "include/str_switch/README.md",
}


def fail(message: str) -> None:
    raise SystemExit(f"documentation-language: {message}")


def project_documents() -> list[pathlib.Path]:
    docs = [path for path in ROOT_DOCS if path.exists()]
    docs.extend(sorted(DOCS_DIR.glob("*.md")))
    if len(docs) < 4:
        fail("не найден ожидаемый набор README/analysis/plan/docs/*.md")
    return docs


def prose_lines(text: str) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    in_fence = False
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        result.append((number, line))
    return result


def validate(path: pathlib.Path) -> None:
    relative = path.relative_to(ROOT).as_posix()
    if relative in EXCLUDED_RELATIVE_PATHS:
        return

    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        fail(f"{relative}: документ должен быть UTF-8: {exc}")

    prose = prose_lines(text)
    prose_text = "\n".join(line for _, line in prose)
    if not CYRILLIC.search(prose_text):
        fail(f"{relative}: нет русского prose")

    words = WORD.findall(prose_text)
    if words:
        cyrillic_words = sum(bool(CYRILLIC.search(word)) for word in words)
        # Guard против случайного возврата целого документа к английскому.
        # Порог намеренно мягкий: API identifiers и англоязычные термины разрешены.
        if cyrillic_words / len(words) < 0.25:
            fail(
                f"{relative}: русский prose составляет слишком малую долю текста "
                f"({cyrillic_words}/{len(words)} слов)"
            )

    for number, line in prose:
        stripped = line.strip()
        if not stripped.startswith("#"):
            continue
        heading = stripped.lstrip("#").strip()
        if LATIN.search(heading) and not CYRILLIC.search(heading):
            fail(
                f"{relative}:{number}: полностью английский Markdown-заголовок: "
                f"{heading!r}"
            )


def main() -> int:
    docs = project_documents()
    for path in docs:
        validate(path)

    print(f"Документация AVM: русский language guard OK ({len(docs)} Markdown-файлов)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
