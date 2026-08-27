# issues/117: read-only `NAME = property(GETTER)` where the getter's whole
# body is `return self.ATTR`. Modelled on voronoi2, INCLUDING the name
# collision that makes the general feature type-directed: `xmin` is a
# property on Bounds and a plain instance field on Window, in one program.
# Reflected ordering is exercised too -- Item defines only __lt__, and
# `a > b` must fall back to `b.__lt__(a)` as Python does.
class Bounds:
    def __init__(self, lo, hi):
        self.__xmin = lo
        self.__xmax = hi
    def _getxmin(self): return self.__xmin
    def _getxmax(self): return self.__xmax
    xmin = property(_getxmin)
    xmax = property(_getxmax)

class Window:
    def __init__(self, xmin):
        self.xmin = xmin          # a PLAIN field of the same name
    def widen(self, d):
        self.xmin = self.xmin - d
        return self.xmin

class Item:
    def __init__(self, k):
        self.k = k
    def __lt__(self, other):
        return self.k < other.k

b = Bounds(3, 9)
print(b.xmin, b.xmax)
w = Window(10)
print(w.xmin, w.widen(4))
print(Item(2) > Item(1), Item(1) > Item(2))
