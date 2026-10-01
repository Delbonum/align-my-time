#!/usr/bin/env python3
"""Checks that every tr ("…") text in plugin/Source has an English entry in Translations.cpp.

Usage: tools/check_translations.py          -> exit 1 and list missing keys
       tools/check_translations.py --keys   -> print all keys (for adding new ones)
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "plugin" / "Source"
TABLE = SOURCE / "Translations.cpp"

LITERAL = r'"(?:[^"\\]|\\.)*"'
CALL = re.compile(r'\btr \(\s*((?:' + LITERAL + r'\s*)+)\)')
ENTRY = re.compile(r'\{\s*((?:' + LITERAL + r'\s*)+),\s*((?:' + LITERAL + r'\s*)+)\}')


def join_literals(text):
    parts = re.findall(LITERAL, text)
    return "".join(bytes(p[1:-1], "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8") for p in parts)


def used_keys():
    keys = {}
    for path in sorted(SOURCE.rglob("*")):
        if path.suffix not in (".cpp", ".h") or path == TABLE:
            continue
        for match in CALL.finditer(path.read_text(encoding="utf-8")):
            keys.setdefault(join_literals(match.group(1)), path.relative_to(ROOT))
    return keys


def table_keys():
    text = TABLE.read_text(encoding="utf-8") if TABLE.exists() else ""
    return {join_literals(m.group(1)) for m in ENTRY.finditer(text)}


def main():
    keys = used_keys()
    if "--keys" in sys.argv:
        for key in keys:
            print(key)
        return 0

    known = table_keys()
    missing = [(k, f) for k, f in keys.items() if k not in known]
    unused = sorted(known - set(keys))
    for key, path in missing:
        print(f"missing translation ({path}): {key!r}")
    for key in unused:
        print(f"unused translation: {key!r}")
    print(f"{len(keys)} texts, {len(missing)} missing, {len(unused)} unused")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
