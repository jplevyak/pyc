# ifa/188: a number and an object in one variable has no representation.
#
# EXPECTED: a compile ERROR naming the union (.check_fail + .check).
# Codegen used to put `x` in a pointer-sized slot and store the int AS a
# pointer, so `0` read as a null receiver and the class-tag dispatch in
# `print(x)` dereferenced it. With -O2 the binary printed `A` for CPython's
# `0`, silently. pyc does not box, so refusing is the only honest answer,
# the same one tests/branch_merged_scalar_union.py gives for {int, str}.
#
# CPython prints 0.
import sys


class A:
    def __str__(self):
        return 'A'


x = 0
if len(sys.argv) > 5:
    x = A()
print(x)
