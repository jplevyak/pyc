# issues/173 #1: `v` is {list, str} -- one heterogeneous list's element, so
# no contour split can separate the two -- and neither list nor str carries
# a run-time class tag, so nothing at run time can answer
# `isinstance(v, list)`. CPython prints 1 0. pyc used to fold the test to
# False and print 0 0 with no diagnostic.
#
# EXPECTED: a compile error naming the test and the untagged member, and a
# non-zero exit (.check_fail + .check) -- the same refusal `len(v)` on this
# value already gets. tests/isinstance_union_decided.py pins the unions
# that CAN be answered.
def check(v):
    if isinstance(v, list):
        return 1
    return 0

lst = [[1, 2], "hello"]
for v in lst:
    print(check(v))
