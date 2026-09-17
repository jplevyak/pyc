# ifa/164, the other half: a nullable pointer needs a POINTEE, so an
# argument whose whole type is `None` is still an error. Only the MIXED
# union {None, T} is sanctioned as a nullable T, and only when the non-nil
# part is itself legal for the primitive. CPython raises
#   TypeError: unsupported operand type(s) for +: 'str' and 'NoneType'
# so refusing this is the right answer, not a leftover of the old check.
def bad():
    return "" + None


print(bad())
