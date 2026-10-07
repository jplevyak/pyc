# String-literal escapes. A backslash before a newline is a line
# continuation and produces nothing (sokoban's `level = """\` + newline: pyc
# kept both, so the board gained a "\" first row). `\012` is a three-digit
# octal escape, which a one-digit `\0` case cut short.
a = """\
abc\
def"""
print(repr(a))
b = "x\
y"
print(repr(b))
c = r"""p\
q"""
print(repr(c))
print(repr("\012"), repr("a\0b"), repr("\101\x41"))
