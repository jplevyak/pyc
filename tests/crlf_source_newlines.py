# issues/124: a source file with CRLF line endings is read with universal
# newlines, as CPython reads it. The `.crlf` sidecar makes the harness
# stage this file with CRLF endings; the copy in the repo stays LF.
a = """one
two"""
print(len(a))
print(a == "one\ntwo")
def f():
    """a docstring
    over two lines"""
    return 1 + \
        2
print(f())
s = "single line"
print(len(s))
