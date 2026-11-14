# issues/007: property setters and deleters. A store `o.x = v` and a
# `del o.x` dispatch on o's class like the read does: C's accessors run for
# a C, the field is written for a D (which has plain fields of the same
# names), and a property with no setter or deleter raises CPython's
# AttributeError. `c.x += 2` is a get then a set.
class C:
    def __init__(self):
        self._x = 3
        self.log = []
    @property
    def x(self): return self._x
    @x.setter
    def x(self, v):
        self.log.append(v)
        self._x = v + 1
    @x.deleter
    def x(self):
        self.log.append(-1)
        self._x = 0
    @property
    def ro(self): return self._x * 10
class D:
    def __init__(self):
        self.x = 1
        self.ro = 2
c = C()
c.x = 5
print(c.x)
c.x += 2
print(c.x, c.log)
del c.x
print(c.x, c.log)
d = D()
d.x = 7
d.ro = 9
d.x += 1

print(d.x, d.ro, c.ro)
try:
    c.ro = 4
except AttributeError as e:
    print("AE", e)
try:
    del c.ro
except AttributeError as e:
    print("AE", e)
for o in [C(), C()]:
    o.x = 10
    print(o.x)
