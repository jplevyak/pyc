# ifa/102: a receiver that may be a list or a tuple has no class tag, but
# lists and tuples share one header, and its capacity bits say which: a
# fixed-arity tuple carries _CG_TUPLE_FIXED_ARITY (ifa/185), a dynamic
# tuple (tuple(xs), a slice) _CG_TUPLE_DYNAMIC, a list neither. The layout
# dispatch tests them, so these calls compile instead of being refused.
# Each union picks its arm at run time, from argv. This copy takes the
# LIST arms when run without arguments; tests/list_tuple_layout_dispatch.py
# takes the tuple arms.
import sys
pick = len(sys.argv) < 2

def show(x):
    print(x, len(x), x[0], x[1:3], x[-1])
    for v in x:
        print(v, end=" ")
    print()

fixed = [1, 2, 3, 4] if pick else (5, 6, 7, 8)
show(fixed)
show(fixed[0:3])
dyn = [9, 8, 7] if pick else tuple([x * 2 for x in range(3)])
show(dyn)
mixed = [1, 2] if pick else (3, 4)
print(mixed)
maybe = None if pick else [4, 5]
if maybe is not None:
    print(len(maybe))
