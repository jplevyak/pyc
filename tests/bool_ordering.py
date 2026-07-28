# bool is an int subtype in Python, so bool values order as 0/1.
# Before this was supported, `max`/`min`/`sorted` over a list of
# comparison results -- or a bare `a > b` on two bools -- dispatched
# to an unresolved `__gt__`/`__lt__` (bool defined only __eq__/__ne__),
# and in a large program that unresolved union destabilized the
# splitter into unrelated NOTYPE cascades (shedskin chess's
# `nonpawnAttacks` -> `max([...==...])` poisoning an unrelated global).

a = True
b = False
print(a > b)          # True
print(a < b)          # False
print(b < a)          # True
print(a >= a)         # True
print(a <= b)         # False
print(b <= b)         # True
print(a >= b)         # True
print(b >= a)         # False

# max/min/sorted over bools -- the shape max() takes internally
# (`m = a[0]; for x in a: if x > m: m = x`)
print(max([False, True, False]))    # True
print(min([True, True, False]))     # False
print(sorted([True, False, True]))  # [False, True, True]

# max over comparison results (chess's nonpawnAttacks shape)
vals = [1, 2, 3, 0, 5]
print(max([v == 0 for v in vals]))  # True  (a 0 is present)
print(max([v > 9 for v in vals]))   # False (none exceed 9)
