#!/usr/bin/env python3
"""Insert or refresh a numbered table of contents in wiki pages.

The table of contents lists every `##` (1, 2, ...) and `###` (1.1, 1.2, ...) heading
of a page as a link to that heading. It is placed between the markers
`<!-- toc -->` and `<!-- /toc -->`, right before the first `##` heading, so it
follows the title block (title, "Inherits", short description). Running the script
again replaces the old table. Headings inside code blocks are ignored.

Usage:
    python truely/wiki-toc.py <wiki-folder-or-page.md>...
Pages starting with "_" (sidebar, footer) are skipped.
"""

import re
import sys
from pathlib import Path

START = "<!-- toc -->"
END = "<!-- /toc -->"


def anchor(text: str, seen: dict[str, int]) -> str:
    # GitHub: lower case, drop everything except letters, digits, spaces, "-" and "_", spaces become "-".
    slug = re.sub(r"[^\w\- ]", "", text.strip().lower()).replace(" ", "-")
    count = seen.get(slug, 0)
    seen[slug] = count + 1
    return slug if count == 0 else f"{slug}-{count}"


def label(text: str) -> str:
    # Keep inline code, drop link syntax: [text](url) -> text.
    return re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text.strip())


def build_toc(lines: list[str]) -> list[str]:
    seen: dict[str, int] = {}
    entries: list[str] = []
    fence = False
    major = minor = 0
    for line in lines:
        if line.lstrip().startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        m = re.match(r"^(#{1,3}) (.+?)\s*$", line)
        if not m:
            continue
        level, text = len(m.group(1)), m.group(2)
        slug = anchor(text, seen)
        if level == 2:
            major, minor = major + 1, 0
            entries.append(f"{major}\\. [{label(text)}](#{slug})")
        elif level == 3 and major:
            minor += 1
            entries.append(f"&emsp;{major}.{minor}\\. [{label(text)}](#{slug})")
    return entries


def process(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    text = re.sub(re.escape(START) + r".*?" + re.escape(END) + r"\n*", "", text, flags=re.S)
    lines = text.split("\n")
    entries = build_toc(lines)
    if not entries:
        return False
    first = next((i for i, line in enumerate(lines) if line.startswith("## ")), None)
    if first is None:
        return False
    # Plain lines with <br> instead of a Markdown list: a list would renumber the
    # entries and merge "1.1" lines into the paragraph of their parent entry.
    body = [e + "<br>" for e in entries[:-1]] + entries[-1:]
    block = [START, "**Contents**", ""] + body + [END, ""]
    new = "\n".join(lines[:first] + block + lines[first:])
    if new != path.read_text(encoding="utf-8"):
        path.write_text(new, encoding="utf-8", newline="\n")
        return True
    return False


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    pages: list[Path] = []
    for arg in sys.argv[1:]:
        p = Path(arg)
        pages += sorted(p.glob("*.md")) if p.is_dir() else [p]
    for page in pages:
        if page.name.startswith("_"):
            continue
        if process(page):
            print(f"updated: {page.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
