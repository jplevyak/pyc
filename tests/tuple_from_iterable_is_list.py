# issues/110: `tuple(iterable)` returns a LIST in pyc.
#
# python_ifa_build_if1.cc lowers a 1-argument `tuple(x)` to
# `x.__pyc_tolist__()`, documented as an "established compromise" on the
# grounds that indexing/iteration/len are identical and only
# printing/hashing differ.
#
# Printing differs visibly -- this program prints [0, 2, 3, 4, 0] where
# CPython prints (0, 2, 3, 4, 0) -- and the type difference propagates:
# it is where shedskin_examples/sunfish's {list, tuple} union comes from,
# which then has no discriminable dispatch and aborts at runtime.
row = [1, 2, 3]
padded = (0,) + tuple(x + 1 for x in row) + (0,)
print(padded)
print(padded[0:2])
