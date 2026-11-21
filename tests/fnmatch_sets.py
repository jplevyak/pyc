# fnmatch's pattern language, CPython's translate exactly (issues/202).
from fnmatch import fnmatch, fnmatchcase, filter
pats = ["*", "*.py", "a?c", "[abc]x", "[!abc]x", "[^a]x", "[]a]x", "[!]a]x", "[a-c]x", "[z-a]x", "[!z-a]x",
        "[-a]x", "[a-]x", "[!-a]x", "[a-c-e]x", "[", "a[", "[!]", "[]", "*[", "a*b*c", "**", "A*", "[a-cx-z]",
        "[\\]x", "?*?", "[!a-c-]", "[a-c-]"]
names = ["", "a", "abc", "ax", "bx", "dx", "^x", "]x", "-x", "ex", "cx", "x.py", "aXc", "[", "a[", "[!]", "abbc",
         "Abc", "z", "y", "\\x", "zz", "-", "!x", "b"]
for p in pats:
    print(repr(p), [n for n in names if fnmatchcase(n, p)], fnmatch("ABC", p.upper()) == fnmatchcase("ABC", p.upper()))
print(filter(["a.py", "B.PY", "c.txt"], "*.py"), fnmatch("B.PY", "*.py"))
