#!/usr/bin/env python3

import pathlib
import re
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "usage: extract_release_notes.py <changelog-path> <tag> <output-path>",
            file=sys.stderr,
        )
        return 1

    changelog_path = pathlib.Path(sys.argv[1])
    raw_tag = sys.argv[2]
    output_path = pathlib.Path(sys.argv[3])
    version = raw_tag[1:] if raw_tag.startswith("v") else raw_tag

    changelog_lines = changelog_path.read_text(encoding="utf-8").splitlines()
    heading_pattern = re.compile(rf"^##\s+Version\s+{re.escape(version)}(?:\b.*)?$")

    start_index = None
    for index, line in enumerate(changelog_lines):
        if heading_pattern.match(line.strip()):
            start_index = index + 1
            break

    if start_index is None:
        print(f"No changelog section found for {raw_tag}", file=sys.stderr)
        return 1

    end_index = len(changelog_lines)
    for index in range(start_index, len(changelog_lines)):
        if changelog_lines[index].startswith("## "):
            end_index = index
            break

    body = "\n".join(changelog_lines[start_index:end_index]).strip()
    if not body:
        print(f"Changelog section for {raw_tag} is empty", file=sys.stderr)
        return 1

    output_path.write_text(body + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())