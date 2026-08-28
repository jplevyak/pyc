# `del` was lowered as `pass` -- silently nothing at all.
# python_ifa_build_if1.cc had
#
#     case PY_pass_stmt:
#     case PY_del_stmt:
#       return 0;
#
# so every `del` in every program was discarded with no diagnostic.
#
# shedskin_examples/block is the victim: its Huffman `iterate()` is
#
#     del c[0]
#     root = iterate(c)
#
# The list never shrank, `len(c) > 1` stayed true, and the binary blew its
# stack after 6800 identical frames -- having compiled with ZERO warnings
# and zero errors on both backends. linalg's `del list1[lasti:n]` and
# sudoku/pygmy's `del self.propQ[0]` are the same shape.
#
# Lowered now the way the equivalent assignment already was:
#   del o[i]    -> o.__delitem__(i)
#   del o[i:j]  -> o[i:j] = []   (__pyc_setslice__ with an empty value,
#                  which is how list.__delitem__ is itself written)
# `del name` and `del o.attr` stay no-ops -- neither has an effect a
# compiled program can observe without a runtime binding model.
#
# The bare `[:]` case needed a second fix. Omitted slice bounds are the
# sentinels INT_MIN/INT_MAX, and `_CG_list_setslice_internal` compared
# them against a uint32 length, so `l > len1` promoted INT_MIN to
# 2147483648 and clamped the lower bound to len1 -- deleting NOTHING.
# `del x[i:j]` with explicit non-negative bounds was unaffected, which is
# why only the `[:]` form exposed it.
a = [10, 20, 30, 40, 50]
del a[0]
print(a)
del a[1:3]
print(a)

b = [1, 2, 3]
del b[:]
print(b, len(b))

c = [1, 2, 3, 4]
del c[2:]
print(c)
del c[:1]
print(c)

# a nested target, the shape sudoku/pygmy use
class holder:
    def __init__(self):
        self.q = [7, 8, 9]

h = holder()
del h.q[1]
print(h.q)

# `del name` is a no-op here, not an error
v = 5
del v
print("done")
