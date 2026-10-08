# ifa/185 step 4: every generated tuple method dispatches on arity -- one
# straight-line body per arity the program builds, plus a runtime loop for
# list-layout tuples (tuple(list)). Mixed arities and layouts in one call
# reach the right body through the runtime arity dispatch.
import copy
ts = [(1, 2), (1, 2, 3), (4,), (), tuple([1, 2]), tuple([4]), tuple([]), (0, 9)]
for x in ts:
    print(x, hash(x) == hash(tuple(list(x))), [x < y for y in ts])
print(sorted(ts))
print((1, (2, 3)), ("a", 1.5, None), (1,), ())
h = (1, "a", 2.5)
print(1 in h, "a" in h, 3 in h, h.count("a"), h.index(2.5), h.index("a", 1))
print(tuple([1, 2, 1]).count(1), tuple([5, 6]).index(6))
try:
    h.index(7)
except ValueError as e:
    print("ValueError", e)
class T:
    def __init__(self, v): self.v = v
a = ((T(1), 1), 2)
b = copy.deepcopy(a)
print(b[0][0] is a[0][0], b[0][0].v, b[1])
print(copy.deepcopy(tuple([1, 2])), copy.deepcopy(()))
import struct
print(struct.pack('<BHH', True, 3, 4))
print(max([(3, "c"), (5, "a"), (5, "b")]), min([(2, 1), (1, 9, 9)]))
