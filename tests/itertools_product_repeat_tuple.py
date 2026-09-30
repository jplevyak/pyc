# issues/171 #5: itertools.product(repeat=N) yields TUPLES, as CPython does
# (they were lists: an observable deviation when printed, hashed or compared).
from itertools import product
ps = list(product([0, 1], repeat=3))
for p in ps:
    print(p)
d = {}
for p in product([1, 2], repeat=2):
    d[p] = len(p)
print(len(d), d[(1, 2)])
for pos, v in zip(ps[1], [7, 8, 9]):
    print(pos, v)
print(ps[0] == (0, 0, 0))
