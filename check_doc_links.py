#!/usr/bin/env python3
"""Check that every relative Markdown link in the repo's docs resolves.

Why this exists: the issue trees archive resolved issues into `closed/`,
and for a long time nothing updated the links pointing AT them. A sweep on
2026-09-02 found 95 of 1100 links broken -- `issues/018` alone was dangling
from six different docs, and four links named files that had never existed
under that name at any point in the repo's history. All of it was invisible
because nothing ever checked.

Checks every git-tracked *.md file. Only relative links to *.md targets are
followed; http(s), mailto, bare anchors, and links to code/other files are
skipped (a link to a source file is checked too if it is relative -- see
--code).

    ./check_doc_links.py          # report and exit 1 if anything dangles
    ./check_doc_links.py --code   # also check relative links to non-.md files
"""

import os
import re
import subprocess
import sys

# [text](target) — no leading `!` (images), and stop at the first `)`.
# Trailing `#anchor` and any ` "title"` are stripped before resolving.
LINK = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")

SKIP_SCHEME = ("http://", "https://", "mailto:", "ftp://", "#")


def tracked_markdown():
    out = subprocess.run(
        ["git", "ls-files", "-z", "*.md"], capture_output=True, text=True, check=True
    ).stdout
    return sorted(p for p in out.split("\0") if p)


def main():
    check_code = "--code" in sys.argv[1:]
    dangling, checked = [], 0

    for path in tracked_markdown():
        try:
            text = open(path, encoding="utf-8").read()
        except (OSError, UnicodeDecodeError) as e:
            print("SKIP %s (%s)" % (path, e), file=sys.stderr)
            continue
        base = os.path.dirname(path)
        for m in LINK.finditer(text):
            target = m.group(1).split("#", 1)[0]
            if not target or target.startswith(SKIP_SCHEME) or os.path.isabs(target):
                continue
            if not check_code and not target.endswith(".md"):
                continue
            checked += 1
            if not os.path.exists(os.path.join(base, target)):
                line = text.count("\n", 0, m.start()) + 1
                dangling.append((path, line, target))

    for path, line, target in dangling:
        print("%s:%d: dangling link -> %s" % (path, line, target))
    print(
        "checked %d relative link(s) in %d file(s): %d dangling"
        % (checked, len(tracked_markdown()), len(dangling))
    )
    return 1 if dangling else 0


if __name__ == "__main__":
    sys.exit(main())
