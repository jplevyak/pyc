# `x in range(...)` / `x not in range(...)`.
#
# python_ifa_build_if1.cc's emit_in_pyda lowers `in` to a direct
# __contains__ dispatch on the right operand with no fallback to the
# iterable protocol, and `range` had no __contains__ -- so every one of
# these was an unresolved dispatch that cascaded into "expression has no
# type" through the whole enclosing function (shedskin_examples othello's
# `x not in range(8)` and sudoku3's `assert x in range(1, 9+1)`).
#
# range.__contains__ is ARITHMETIC, matching CPython: O(1) for an int, and
# non-consuming, which matters because pyc's range is its own iterator
# (__iter__ returns `this`). The last block below is what pins that -- a
# scanning implementation would leave the range exhausted and sum 0.

def probe(lo, hi, st, v):
    return v in range(lo, hi, st)

# step 1, including both boundaries
for v in [0, 1, 4, 9, 10]:
    print(probe(1, 10, 1, v))

# step > 1: only every other value is a member
for v in [0, 2, 3, 8, 10]:
    print(probe(0, 10, 2, v))

# negative step -- the modulo has to stay sign-correct
for v in [0, 1, 2, 8, 9, 10]:
    print(probe(10, 0, -2, v))

# empty range contains nothing
for v in [4, 5, 6]:
    print(probe(5, 5, 1, v))

# a step that overshoots the stop
for v in [4, 5, 6, 7, 8]:
    print(probe(5, 8, 3, v))

# the one- and two-argument forms
print(3 in range(10))
print(10 in range(10))
print(-1 in range(10))
print(7 not in range(10))
print(11 not in range(10))

# __contains__ must not consume the range: 0+1+2+3+4 == 10
r = range(5)
print(3 in r)
total = 0
for i in r:
    total = total + i
print(total)
