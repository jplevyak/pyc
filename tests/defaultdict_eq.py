# collections.defaultdict compares equal to an equal defaultdict or dict,
# as in CPython (life's `board in history`: it compared by identity, so
# process() never found a repeat and looped forever). A missing key with
# no factory is a KeyError rather than an inserted None.
from collections import defaultdict

a = defaultdict(int)
a[0, 0] = 1
x = a[5, 5]
print(len(a), x, a[0, 0])
b = defaultdict(None, a)
print(b == a, a == b, len(b))
c = defaultdict(int, a)
c[0, 0] = 0
print(c == a, c != a, a in [b], c in [b], a in [c])
d = {(0, 0): 1, (5, 5): 0}
print(d == a, a == d)
n = defaultdict(None, {1: 2})
print(1 in n, 3 in n, n.get(3, -1), n.get(1, -1))
del n[1]
print(len(n))
