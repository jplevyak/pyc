# Regression: two functions append-building lists of DIFFERENT tuple
# types in one program (one indexed, one iterated). The generic
# list's element type is then a SUM of the two record types with no
# compile-time size, and sizeof_element emitted 0 -- list::append
# resized with element size 0, storage never grew, and reads
# returned null at runtime with a CLEAN COMPILE (issue 025 tuple-list
# soundness bug). Fixed: SUM-of-records elements are boxed pointers,
# so sizeof_element emits pointer size (cg.cc + cg_emit_llvm.cc).
# Also exercises zip and enumerate together, which was the original
# trigger.
def mkints(a, b):
    r = []
    i = 0
    while i < len(a):
        r.append((a[i], b[i]))
        i = i + 1
    return r

def mkstrs(seq):
    r = []
    i = 0
    for x in seq:
        r.append((i, x))
        i = i + 1
    return r

def main():
    print(mkints([3, 1], [10, 20])[1][1])   # 20
    for i, v in mkstrs(["a", "b"]):
        print(i, v)
    z = zip([1, 2], [30, 40])
    print(z[0][1])                           # 30
    for i, v in enumerate(["x", "y"]):
        print(i, v)

main()
