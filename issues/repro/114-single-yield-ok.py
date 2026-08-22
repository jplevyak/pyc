# issues/114 repro A -- FIXED 2026-08-21.
#
# A generator yielding a non-int. Before the fix the value channel was
# int-typed, so this handed back a reinterpreted pointer and printing x
# printed an address, with no diagnostic. len, indexing and equality
# are all correct now.
#
#   2 2
#   True True
#   True
#
# Covered by tests/generator_yields_nonint.py; kept here as the
# smallest single-yield case.


def gen():
    yield (1, 2)


t = (1, 2)
for x in gen():
    print(len(x), len(t))
    print(x[0] == t[0], x[1] == t[1])
    print(x == t)
