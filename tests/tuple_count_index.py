# tuple.count and tuple.index were missing from __pyc__: `t.count(x)`
# was "unresolved member 'count' of class 'tuple'".
t = (1, 2, 3, 2, 1, 2)
print(t.count(2), t.count(7))
print(t.index(2), t.index(2, 2), t.index(2, -2), t.index(3, 0, 3))
try:
    t.index(3, 3)
except ValueError:
    print("ValueError")
try:
    t.index(9)
except ValueError:
    print("ValueError")

# runtime-length tuples (list layout): tuple(list) and a slice
r = tuple([5, 6, 5, 5])
print(r.count(5), r.index(6))
s = r[1:]
print(s.count(5), s.index(5))

# heterogeneous elements
h = (1, "a", 2.5, "a")
print(h.count("a"), h.index("a"), h.index(2.5), h.count(1))

# strings and nested tuples
w = ("x", "y", "x")
print(w.count("x"), w.index("y"))
p = ((1, 2), (3, 4), (1, 2))
print(p.count((1, 2)), p.index((3, 4)))
print(().count(1))

# a {None, tuple} receiver, now that the member exists
import random
x = None
if random.random() > 2.0:
    x = None
else:
    x = (4, 4, 1)
print(x.count(4), x.index(1))

# `in` on a heterogeneous tuple (it needed the same unroll)
print("a" in h, 3 in h, 2.5 in h, None in h)

# past the unrolled prefix: a runtime-length tuple longer than any literal
big = tuple(range(20))
print(big.count(17), big.index(17), big.index(3, 2, 10), 19 in big, 20 in big)
try:
    big.index(17, 0, 17)
except ValueError:
    print("ValueError")
