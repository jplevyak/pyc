# ROADMAP 6.1: *args in definitions -- functions, methods, a fixed
# parameter before *args, mixed element types, forwarding, an explicit
# tuple in the rest position, and recursion.
def f(*args):
    print(len(args))
    for a in args:
        print(a)

f(1, 2, 3)
f()

def g(x, *rest):
    print(x)
    print(rest)

g(1, 2, 3)
g(10)
class C:
    def __init__(self, base):
        self.base = base
    def total(self, *nums):
        s = self.base
        for n in nums:
            s += n
        return s
    def show(self, label, *items):
        return label + ":" + ",".join([str(i) for i in items])

c = C(100)
print(c.total(), c.total(1), c.total(1, 2, 3))
print(c.show("a"), c.show("b", 1, 2), c.show("c", "x", "y", "z"))

def mixed(*args):
    return args
print(mixed(1, "two", 3.0))

def fwd(x, *args):
    return inner(x, args)
def inner(x, *more):
    return (x, more)
print(fwd(1, 2, 3))
print(inner(0, (5, 6)))

def count(*a):
    return len(a)
print(count(*[]) if False else count(), count(None), count([1, 2]))

def rec(n, *acc):
    if n == 0:
        return acc
    return rec(n - 1, n)
print(rec(3))
