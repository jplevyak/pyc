# str's ASCII classification methods, expandtabs, and splitlines with CPython's
# line boundaries and keepends (issues/205).
for s in ["", "a", "Z", "ab1", "a b", "_", "1", "12", "a_1", "1a", "\t", "~", "Hello World", "Hello world", "HELLO", "A1 B2", "aB", " Ab", "x\x7f", "\x00"]:
    print(repr(s), s.isalpha(), s.isalnum(), s.isdecimal(), s.isnumeric(), s.isprintable(), s.isascii(), s.isidentifier(), s.istitle(), s.isdigit())
for s in ["a\tb\tc", "ab\ncd\r\nef", "x\x0by\x0cz\x1cw\r", "", "\n", "a\n\nb\n", "12345\t6\n\tx"]:
    print(repr(s.expandtabs(4)), repr(s.expandtabs()), s.splitlines(), s.splitlines(True), s.splitlines(keepends=True))
