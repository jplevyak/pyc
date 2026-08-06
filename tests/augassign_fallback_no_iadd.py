# issues/034: CPython falls back from __i<op>__ to __<op>__ (then
# reassigns) when a class defines only the latter -- `c += x` on a
# class with __add__ alone is exactly `c = c.__add__(x)`, not an
# error. pyc's augmented-assignment lowering used to always send
# __i<op>__ directly with no such fallback. Found via
# shedskin_examples/yopyra/yopyra.py's color/punto3d classes, which
# each define __add__ (and friends) but never __iadd__ etc.
class Vec:
    def __init__(self, x):
        self.x = x
    def __add__(self, other):
        return Vec(self.x + other.x)
    def __sub__(self, other):
        return Vec(self.x - other.x)
    def __mul__(self, n):
        return Vec(self.x * n)
    def __str__(self):
        return "Vec(%d)" % self.x

a = Vec(1)
a += Vec(2)
print(a)
a -= Vec(1)
print(a)
a *= 3
print(a)

# A class that DOES define its own __iadd__ must still use it (no
# regression to the __add__ fallback overriding a real override).
class Counter:
    def __init__(self, n):
        self.n = n
    def __add__(self, other):
        return Counter(self.n + other)
    def __iadd__(self, other):
        self.n += other * 10
        return self
    def __str__(self):
        return "Counter(%d)" % self.n

c = Counter(1)
c += 2
print(c)
