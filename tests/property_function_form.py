# issues/007: `NAME = property(fget, fset, fdel, doc)` with getters that are
# not a bare field read. The property holds the FUNCTIONS it was given, so
# S overriding _get does not change the inherited `v`; S replaces `g` with
# its own property and extends `v` with a deleter (`@B.v.deleter`).
def outside(self): return self._v * 3
class B:
    def __init__(self): self._v = 2
    def _get(self): return self._v + 100
    def _set(self, v): self._v = v * 2
    def _del(self): self._v = -1
    v = property(_get, _set, _del, "doc")
    g = property(_get)
    k = property(fget=_get, fset=_set)
    o = property(outside)
class S(B):
    def _get(self): return 0
    @property
    def g(self): return 55
    @B.v.deleter
    def v(self): self._v = -7
b = B()
print(b.v, b.g, b.k, b.o)
b.v = 5
print(b.v)
b.k = 1
print(b.k)
del b.v
print(b.v)
s = S()
print(s.v, s.g, s.k)
s.v = 4
del s.v
print(s.v)
try:
    b.g = 1
except AttributeError as e:
    print("AE", e)
try:
    del b.k
except AttributeError as e:
    print("AE", e)
