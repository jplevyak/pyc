# ifa/issues/109: MIXED-ARITY TUPLES DO cause a real failure -- this is
# shedskin_examples/sunfish's crash, reduced to seven lines.
#
# `pst[k]` is reassigned with a different-LENGTH tuple inside the loop, so
# the dict's value type (and hence `table`) is {tuple(N), tuple(M)} --
# same element type, different arity. Slicing that value needs
# __pyc_getslice__, and pyc has cloned it per receiver arity:
#
#   DISPATCH FAIL: cand=__pyc_getslice__ cand=__pyc_getslice__ r1=_:?
#
# Codegen cannot discriminate the two clones, emits
# assert(!"runtime error: matching function not found"), and the binary
# aborts (SIGABRT) where CPython prints (0, 1, 2, 0) (0, 5, 6, 0).
#
# This is exactly what shedskin's variable-length homogeneous `tuple<T>`
# represents as ONE type and pyc's fixed-arity records cannot --
# ifa/issues/104. That issue concluded mixed arity "causes no failures in
# the corpus"; the conclusion was wrong because its probe counted
# VIOLATIONS, and this failure produces none.
pst = {"P": (1, 2, 3, 4), "N": (5, 6, 7, 8)}
for k, table in list(pst.items()):
    pst[k] = table[0:2]
    pst[k] = (0,) + pst[k] + (0,)
print(pst["P"], pst["N"])
