# issues/110: `tuple(iterable)` returned a LIST in pyc.
#
# python_ifa_build_if1.cc lowered a 1-argument `tuple(x)` to
# `x.__pyc_tolist__()`, an "established compromise" on the grounds that
# indexing/iteration/len are identical and only printing/hashing differ.
# Printing differed visibly -- [0, 2, 3, 4, 0] for CPython's
# (0, 2, 3, 4, 0) -- and the type difference propagated into
# shedskin_examples/sunfish's {list, tuple} union. It is now a real
# runtime-length tuple (make_seq, the default since 2026-09-28;
# PYC_MAKESEQ=0 restores the list).
row = [1, 2, 3]
padded = (0,) + tuple(x + 1 for x in row) + (0,)
print(padded)
print(padded[0:2])
