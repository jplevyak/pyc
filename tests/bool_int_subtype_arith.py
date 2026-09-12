# bool is an int subtype in Python, for arithmetic as well as ordering.
# Without bool.__add__/__sub__/__mul__/__xor__ every expression below was
# an unresolved dispatch that cascaded through the enclosing function:
# timsort's `return (a > b) - (a < b)` three-way compare, quameon's
# `not(x^y)`, softrender's `if currentInside ^ previousInside:`.
#
# The mixed bool-with-int cases are the point of the implementation shape.
# Reducing `self` to the literal 0 or 1 and letting the NUMERIC operator
# run gets `True + 5 == 6` right; returning bool-shaped literals from a
# branch on self would have returned 2 and compiled clean.
#
# `^` additionally has to preserve CPython's return TYPE: bool ^ bool is a
# bool (prints False, not 0) while bool ^ int is an int.

def gt(a, b):
    return a > b

def lt(a, b):
    return a < b

# the three-way compare, the shape timsort uses
def cmp3(a, b):
    return (a > b) - (a < b)

print(cmp3(3, 2))
print(cmp3(2, 3))
print(cmp3(2, 2))

# bool OP bool -- arithmetic widens to int
t = gt(3, 2)
f = lt(3, 2)
print(t + t)
print(t + f)
print(f + f)
print(t - t)
print(t - f)
print(f - t)
print(t * t)
print(t * f)

# bool ^ bool stays a bool, so these print True/False not 1/0
print(t ^ t)
print(t ^ f)
print(f ^ t)
print(f ^ f)
print(not (t ^ f))

# mixed bool with int -- must follow int arithmetic, not bool shapes
print(t + 5)
print(f + 5)
print(t - 5)
print(t * 7)
print(f * 7)

# bool ^ int is an int
print(t ^ 3)
print(f ^ 3)

# and the pre-existing bool & / | still hold
print(t & f)
print(t | f)
