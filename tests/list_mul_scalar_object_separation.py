# ifa/issues/133 (and issues/039): a `[None] * n` list must not share a
# CreationSet with a list whose elements are a scalar.
#
# This is `richards` reduced. That program has exactly two list
# multiplications --
#
#     self.data    = [0]    * BUFSIZE       # element: int64
#     self.taskTab = [None] * TASKTABSIZE   # element: None | task classes
#
# -- and under PYC_CSDCPA1 every list starts on ONE CreationSet, so their
# element channels merge. `richards` then warns
#
#     illegal call argument type 't' illegal: ( __pyc_None_type__ int64 Packet )
#
# where the default arm says plainly `Packet`, and the binary SIGSEGVs with
# an int64 dereferenced as a task pointer. This file reproduces the union
# shape in 25 lines.
#
# WHICH HALF MATTERS, probed by substitution (2026-09-09). The name says
# "mul" because it mirrors richards, but the multiplication on the SCALAR
# side is not what does it:
#
#   Buf.data          Area.taskTab     PYC_CSDCPA1=2
#   ---------------   --------------   -------------
#   [0] * 4           [None] * 4       FAILS   <- richards' shape, this file
#   [0]               [None] * 4       FAILS
#   []; append(0)     [None] * 4       clean
#   [0]               [None]           clean
#
# So `[None] * n` is the necessary ingredient -- issues/039's subject
# exactly -- and it needs a LITERAL-POSITIONAL scalar on the other side.
# Route the same int through `append` and the merge does not happen, which
# says the two content channels (positional `cs->vars` vs the generic
# element, ifa/104) are not equally exposed here. Do not read this file as
# "sequence multiplication is a distinct creation route"; an earlier
# version of this comment claimed that and the probe above refutes it.
#
# Keep it alongside tests/two_list_element_separation.py, which reaches one
# shared CreationSet through `append` on two `[]` literals: that one is the
# channel this file finds CLEAN, so the two cover opposite sides of the
# same defect and a fix wants both.
#
# Unlike issues/039's list_mul_element_cross_contamination.py -- which
# merges two OBJECT lists and so loses precision without losing
# representability -- this mix is {int64, object} and has no representation
# at all. It is a wrong answer, not a wide one. Swapping the `0` for a
# string sharpens it from a warning to
# `error: expression has mixed basic types:( __pyc_None_type__ int64 str )`.
#
# STATUS 2026-09-10: passes on BOTH arms. It failed under PYC_CSDCPA1=2
# when written; ifa/133's `nilstore` fix (count a None store as a store --
# `compute_setters` was dropping it because `->type` projects a lone nil to
# bottom) resolved it, and disabling that fix with PYC_NILSTORE=0 brings the
# diagnostic straight back. So this is now a REGRESSION GUARD for that fix
# rather than a pin on outstanding work.
#
# It does NOT cover `bh`, despite the shared shape -- see
# tests/arity1_literal_shares_contour.py, which has a str on the far side
# and an unmultiplied literal, and which the nilstore fix does not reach.
class Task:
    def __init__(self, n):
        self.ident = n

    def run(self):
        return self.ident


class Buf:
    def __init__(self):
        self.data = [0] * 4          # element: int64


class Area:
    def __init__(self):
        self.taskTab = [None] * 4    # element: None | Task


def main():
    b = Buf()
    b.data[0] = 7
    a = Area()
    a.taskTab[0] = Task(3)
    t = a.taskTab[0]
    print(b.data[0], t.run())


main()
