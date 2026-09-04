# ifa/issues/118: `copy.copy` of a GENERIC list returned an ALIAS.
#
# cg.cc's P_prim_copy emits identity for any destination that is not
# Type_RECORD -- correct for scalars and immutable strings, wrong for a
# list, which is Type_PRIMITIVE. A small list LITERAL happens to get
# record shape and copied correctly, which is why this hid: only a list
# that stays generic (here, built from a tuple) aliased.
#
# chess depended on it: legalMoves does `board2 = copy(board)` and then
# mutates board2, so every search corrupted the caller's board and
# returned an immediate beta cutoff.
from copy import copy

setup = (1, 2, 3, 4)
b = list(setup)
c = copy(b)
c[1] = 99
print(b)
print(c)

# The literal case, which already worked -- keep it working.
xs = [1, 2, 3]
ys = copy(xs)
ys[0] = 77
print(xs)
print(ys)

# Copies must be independent of each other, not just of the original.
d = copy(b)
d[0] = -1
print(b)
print(c)
print(d)
