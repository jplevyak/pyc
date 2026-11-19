# issues/007: a decorator on a METHOD is applied (it used to be computed
# and dropped), including under @property, stacked, parameterized, and on
# a recursive method, whose recursion goes back through the decorator.
# D overrides an inherited decorated method.
def deco(f):
    def w(self):
        return f(self) + 1
    return w
def scaled(k):
    def d(f):
        def w(self, a, b=10):
            return f(self, a, b) * k
        return w
    return d
calls = []
def counted(f):
    def w(self, n):
        calls.append(n)
        return f(self, n)
    return w
class C:
    def __init__(self): self._x = 3
    @deco
    def m(self): return self._x
    @property
    @deco
    def x(self): return self._x
    @scaled(10)
    def add(self, a, b=7): return self._x + a + b
    @deco
    @deco
    def twice(self): return 0
    @counted
    def fact(self, n):
        if n <= 1: return 1
        return n * self.fact(n - 1)
class D(C):
    def m(self): return 100
c = C()
print(c.m(), c.x, c.add(1, 2), c.twice(), c.fact(4), calls)
print(D().m(), D().x)
try:
    c.x = 3
except AttributeError as e:
    print("AE", e)
