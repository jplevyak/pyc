# textwrap, as CPython 3.12's: wrap, fill, shorten, dedent, indent (issues/205).
import textwrap
from textwrap import TextWrapper, dedent, indent

texts = [
    "Look, goof-ball -- use the -b option!",
    "The quick brown fox jumps over the lazy dog. It was a well-known fact that hyper-text-markup was co-opted by mid-1990s web-designers.",
    "  leading and trailing   spaces  ",
    "tab\tseparated\twords\tand a verylongwordthatdoesnotfitanywhereatall in it",
    "Hello there.\nThis is a sentence. And another one! Is it? yes.",
    "x--y a--b ---c 3-d e-f-g ab-cd abc-de-fg-hi re-x a-bcd",
    "",
    "     ",
    "one",
    "em--dash and foo--bar.--baz and !--x",
    "_under-score_ 9-9 a_-b well,-known",
]
for t in texts:
    for w in [1, 5, 12, 20, 40]:
        print(w, textwrap.wrap(t, w))
    print(repr(textwrap.fill(t, 25, initial_indent="* ", subsequent_indent="  ")))
    print(textwrap.wrap(t, 15, break_on_hyphens=False), textwrap.wrap(t, 10, break_long_words=False))
    print(textwrap.wrap(t, 18, drop_whitespace=False), textwrap.wrap(t, 30, fix_sentence_endings=True))
    print(textwrap.wrap(t, 20, max_lines=2), textwrap.wrap(t, 20, max_lines=1, placeholder="..."))
    print(repr(textwrap.shorten(t, 20)), repr(textwrap.shorten(t, 30, placeholder=" (more)")))
    print(textwrap.wrap(t, 12, expand_tabs=False), textwrap.wrap(t, 12, tabsize=2), textwrap.wrap(t, 12, replace_whitespace=False))
tw = TextWrapper(width=16, initial_indent="> ")
print(tw.wrap("a b c d e f g h i j k l m n o p"), tw.fill("short"))
print(repr(dedent("    def f():\n        return 1\n    \n  x")), repr(dedent("\thello\n  \tworld\n")),
      repr(dedent("  a\n    b\n   \n\n  c\n")), repr(dedent("no indent\n  x")), repr(dedent("  \t a\n  \t b")))
print(repr(indent("a\n\n b\n  \nc", "> ")), repr(indent("a\nb\n", "+", lambda l: l.startswith("b"))))
try:
    textwrap.wrap("abc", 0)
except ValueError as e:
    print("ValueError", e)
try:
    textwrap.wrap("abc def", 3, max_lines=1)
except ValueError as e:
    print("ValueError", e)
