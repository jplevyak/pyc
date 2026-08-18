# ifa/issues/109: tuple slicing is UNIMPLEMENTED -- shedskin_examples/
# sunfish's crash, reduced.
#
# `class tuple` in __pyc__/04_sequence.py has no __pyc_getslice__ (only
# `class list` does), so any tuple slice aborts with
# "runtime error: list index type mismatch". Two lines suffice:
#
#     t = (1, 2, 3, 4); print(t[0:2])       # SIGABRT; CPython: (1, 2)
#
# This file keeps sunfish's fuller shape (dict values reassigned with a
# different-length tuple) because that is how it was found -- but the
# mixed ARITY is incidental: the same program with uniform arity, and a
# plain tuple with a constant slice, both abort identically. An earlier
# revision of this test blamed arity; it was wrong.
pst = {"P": (1, 2, 3, 4), "N": (5, 6, 7, 8)}
for k, table in list(pst.items()):
    pst[k] = table[0:2]
    pst[k] = (0,) + pst[k] + (0,)
print(pst["P"], pst["N"])
