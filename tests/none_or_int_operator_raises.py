# issues/183: `x + 5` with x {None, int} must raise TypeError when x is None,
# or be refused. pyc compiled it in every mode and read None as 0.
#
# EXPECTED: a compile error naming the {None, int} operand, in every mode
# (.check_fail + .check). x really is None on one run and an int on another,
# so no contour split separates them, and an int slot has no bit pattern
# left for None. A union pyc invents instead (two objects from one creation
# point, one holding None) is split and compiles.
import sys
x = None if len(sys.argv) > 5 else 1
y = 1 if len(sys.argv) > 5 else None
print(x + 5)
try:
    print(y + 5)
except TypeError:
    print("TypeError")
