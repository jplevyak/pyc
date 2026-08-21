# issues/114 repro A -- WORKS on this branch, fails on main.
#
# A generator yielding a non-int. On main the value channel is
# int-typed, so this hands back a reinterpreted pointer. With the
# branch's changes the tuple survives: len, indexing and equality are
# all correct.
#
#   expected (and produced on this branch):
#     2 2
#     True True
#     True


def gen():
    yield (1, 2)


t = (1, 2)
for x in gen():
    print(len(x), len(t))
    print(x[0] == t[0], x[1] == t[1])
    print(x == t)
