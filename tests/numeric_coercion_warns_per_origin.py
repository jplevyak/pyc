# ifa/189: a permissive int/float coercion warns once, where the int and the
# float meet, and names the lines they come from. It used to warn again at
# every expression computed from the widened value (doom: 271 warnings from
# 11 merges), though those deviate only because the origin did.
import sys


def wall(h, z, portal):
    if portal:
        back = h - z
    else:
        back = 0
    top = back / 2.0 + 1
    bottom = back * 3 - 1
    mid = (top + bottom) / 2
    return top, bottom > 0, mid


# Only values CPython also prints as floats (or bools): the point here is the
# warning, not the deviation it warns about.
print(wall(3.5, 1.0, len(sys.argv) > 3))
print(wall(3.5, 1.0, len(sys.argv) < 3))
