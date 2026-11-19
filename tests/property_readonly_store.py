# issues/007, issues/117: `NAME = property(GETTER)` whose getter is
# `return self._field` stays a plain field read, and is still read-only:
# a store from outside raises, while the class's own stores to the field
# work. E has plain fields of the same names.
class S:
    def __init__(self):
        self._lo = 1
        self._hi = 9
    def _getlo(self): return self._lo
    def _gethi(self): return self._hi
    def grow(self):
        self._hi += 1
        self._lo = self._lo - 1
    lo = property(_getlo)
    hi = property(_gethi)
class E:
    def __init__(self):
        self.lo = 5
        self.hi = 6
s = S()
e = E()
s.grow()
e.lo = 50
e.hi += 1
print(s.lo, s.hi, e.lo, e.hi)
try:
    s.lo = 3
except AttributeError as x:
    print("AE", x)
try:
    s.hi += 3
except AttributeError as x:
    print("AE", x)
print(s.lo, s.hi)
