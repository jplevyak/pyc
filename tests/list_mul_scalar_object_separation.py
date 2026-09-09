# ifa/issues/133 (and issues/039): two `[X] * n` lists whose elements are a
# SCALAR and an OBJECT must not share a CreationSet.
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
# where the default arm says plainly `Packet`, and the binary SIGSEGVs.
# This file reproduces the union shape in 25 lines.
#
# It is a different CREATION ROUTE to the same defect as
# tests/two_list_element_separation.py, which reaches one shared
# CreationSet through `append` on two `[]` literals. Keep both: the
# sequence-multiply route builds its element type from the multiplied
# operand rather than from a later write, so a fix that only follows
# writes will pass one and fail the other.
#
# Unlike issues/039's list_mul_element_cross_contamination.py -- which
# merges two OBJECT lists and so loses precision without losing
# representability -- this mix is {int64, object} and has no
# representation at all. It is a wrong answer, not a wide one.
#
# STATUS 2026-09-09: passes at the DEFAULT, fails under PYC_CSDCPA1=2 with
# `illegal call argument type 't' illegal: ( __pyc_None_type__ int64 )`.
# Guards the default arm and pins the flag arm's remaining work.
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
