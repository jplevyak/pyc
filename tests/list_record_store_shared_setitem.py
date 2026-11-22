# A list display of mixed types is a record, typed per position, as long as
# every index is a constant. Two stores of the same types at different
# constant indices share one list.__setitem__ contour, which merged their
# keys: neither store was constant-indexed, each wrote every position, and
# position 6 read back {int, list}, refused as having no representation.
# CONST_DEMAND now follows a store's key, as it does a read's, and splits
# the contour on it (ifa/analysis/fa.cc, set_index_tval_store).
def get():
    cc = []
    for i in range(4):
        cc.append(i)
    return [1, 2, 3, 4, 5, 6, cc]


n = int("9")
a = get()
a[3] = a[3] & 1
a[4] = n
a[6][2] = n
print(a[0], a[3], a[4], a[5], a[6], len(a[6]))
