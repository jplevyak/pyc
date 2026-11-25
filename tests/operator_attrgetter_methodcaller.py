# operator.attrgetter and methodcaller with constant names (issues/216):
# each call site is lowered to a class of its own; the arguments of
# methodcaller are evaluated once, where it is made, as in CPython.
import operator
from operator import attrgetter, methodcaller


class Pt:
    def __init__(self, x, y):
        self.x = x
        self.y = y
        self.tag = "p%d" % x

    def scaled(self, k, off=0):
        return self.x * k + off


class Box:
    def __init__(self, p):
        self.p = p


pts = [Pt(3, 1), Pt(1, 2), Pt(2, 0)]
print([p.x for p in sorted(pts, key=attrgetter("x"))], [p.tag for p in sorted(pts, key=operator.attrgetter("y"))])
print(attrgetter("x", "tag")(pts[0]), attrgetter("p.y")(Box(pts[1])), list(map(attrgetter("tag"), pts)))
print(repr(attrgetter("x")), repr(attrgetter("p.x", "p.y")))
calls = []


def arg(v):
    calls.append(v)
    return v


mc = methodcaller("scaled", arg(10), off=arg(5))
print(calls, [mc(p) for p in pts], calls)
print(methodcaller("upper")("abc"), methodcaller("replace", "a", "b")("banana"), methodcaller("split", ",")("a,b"))
print(repr(mc), repr(methodcaller("upper")), repr(operator.methodcaller("m", 1, "z", k=2.5)))
print(max(pts, key=attrgetter("y")).tag, sorted(["b", "A", "c"], key=methodcaller("lower")))
