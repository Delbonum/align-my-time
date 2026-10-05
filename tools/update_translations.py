#!/usr/bin/env python3
"""Rewrites plugin/Source/Translations.cpp: keeps existing entries that are still used, adds new ones.

Usage: tools/update_translations.py new.json
       new.json = {"German text": "English text", ...} for the texts check_translations.py reports missing.
"""
import json
import sys

import check_translations as ct


def main():
    additions = json.load(open(sys.argv[1], encoding="utf-8")) if len(sys.argv) > 1 else {}
    text = ct.TABLE.read_text(encoding="utf-8")
    existing = {ct.join_literals(m.group(1)): ct.join_literals(m.group(2)) for m in ct.ENTRY.finditer(text)}
    existing.update(additions)

    keys = ct.used_keys()
    missing = [k for k in keys if k not in existing]
    if missing:
        for k in missing:
            print(f"still missing: {k!r}")
        return 1

    lit = lambda s: json.dumps(s, ensure_ascii=False)
    lines = ["#include <string>", "#include <unordered_map>", "", "namespace amt::plugin", "{", "",
             "// German UI text (as written in the sources) -> English. Checked by tools/check_translations.py.",
             "const std::unordered_map<std::string, const char*>& englishTranslations()", "{",
             "    static const std::unordered_map<std::string, const char*> table {"]
    lines += [f"        {{ {lit(k)}, {lit(existing[k])} }}," for k in keys]
    lines += ["    };", "    return table;", "}", "", "} // namespace amt::plugin", ""]
    ct.TABLE.write_text("\n".join(lines), encoding="utf-8")
    print(f"{len(keys)} translations written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
