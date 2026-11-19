# issues/007: class-based decorators. `@W` binds the name to a W instance,
# and calling it runs W.__call__. Two instances wrap functions of different
# types; a class decorator can take arguments and stack with function ones.
class Counter:
    def __init__(self, f):
        self.f = f
        self.calls = 0
    def __call__(self, a, b):
        self.calls += 1
        return self.f(a, b)
@Counter
def add(a, b): return a + b
@Counter
def cat(a, b): return a + b
print(add(1, 2), cat("a", "b"), add(3, 4), add.calls, cat.calls)

class Plus1:
    def __init__(self, f):
        self.f = f
    def __call__(self, x):
        return self.f(x) + 1
class Rep:
    def __init__(self, n):
        self.n = n
    def __call__(self, f):
        return Plus1(f)
def twice(f):
    def w(x): return f(f(x))
    return w
@Rep(3)
def g(x): return x * 2
@twice
@Plus1
def h(x): return x * 2
@Plus1
@twice
def k(x): return x * 2
print(g(3), h(3), k(3))
