# issues/018: a branch-merged {list, float} union in one variable.
#
# EXPECTED: a compile ERROR naming the union, and a non-zero exit
# (.check_fail + .check). `x` holds {list, float64}; `+` resolves to
# the CONTAINER `__add__`, whose receiver may be the scalar. pyc does
# not box, so that union has no representation and refusing is the only
# honest answer -- the same conclusion as the scalar-only shape in
# tests/branch_merged_scalar_union.py.
#
# It used to be a `.known_issue`, which implied an intended fix. There
# is none to make without boxing. What DID need fixing was the message:
# it used to read
#
#   fail: ../../__pyc__.py:1155: internal: sizeof_element of
#   non-container type '<anonymous>' (in __add__) ...
#
# -- an "internal" error, at a line inside the builtin library, naming
# the union as '<anonymous>'. All three were misleading.
#
# NOT plcfrs's failure, despite the old message resembling it -- see
# ifa/issues/105. shedskin rejects this program too (its generated C++
# does not compile) and shedskin compiles plcfrs, so plcfrs cannot
# contain this shape.
#
# The essential ingredient is that the union forms in ONE VARIABLE. The
# same two types reaching `__add__` through SEPARATE call sites --
#
#     def add(a, b): return a + b
#     add([1, 2], [3]); add(3.5, 1.5)
#
# -- compiles and runs fine, because FA gives those calls separate
# contours. So this is not "pyc cannot mix list and float"; it is the
# branch merge defeating contour separation.
#
# CPython prints 7.0.
import sys
x = [1] if len(sys.argv) > 1 else 3.5
print(x + x)
