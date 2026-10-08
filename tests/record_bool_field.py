# A tuple with an int and a run-time bool, `{ i64, i1 }` on the LLVM
# backend: a constant-index field was addressed as an array of the
# element's type, so field 1 was read at byte offset 1 and `mk(True)`
# came out (2, False).
def mk(root):
    return (2, root)

a = mk(True)
b = mk(False)
print(a == (2, True), b == (2, False), a[1], b[1], a, b)
def key(d, root):
    return (d, (d, root))

t = {}
t[key(3, True)] = "x"
t[key(4, False)] = "y"
print((3, (3, True)) in t, (4, (4, False)) in t, (3, (3, False)) in t, t[3, (3, True)])
