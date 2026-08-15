# issues/048: two None-initialised instance fields that later hold ints.
# pyc compiles this with ZERO diagnostics and the binary aborts with
# "matching function not found"; CPython prints "1 2".
#
# Both fields do get their own slots, so this is NOT issues/046's elided-
# slot confusion. Each field's type is the union None|int64, which puts it
# in the None-boxing family with issues/018 / 030 / 035 -- but with no
# container, no in-place mutation and no heterogeneous element type, so it
# is the smallest witness in that family.
#
# The check files below describe the CORRECT behaviour; the .known_issue
# tag is what keeps this from failing the suite. Delete the tag when fixed.
class V:
    def __init__(self):
        self.a = None
        self.b = None

v = V()
v.a = 1
v.b = 2
print(v.a, v.b)
