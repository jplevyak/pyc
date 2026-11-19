# issues/189: `f(*t)` binds the tuple's elements as positionals, by the
# tuple's arity: to named parameters, then defaults, then `*args`, with
# keywords after it, into methods and constructors.
def f(a, b):
    return a - b

t = (5, 2)
print(f(*t))

def g(a, b, c=10):
    return a + b * c
print(g(*t))
print(g(1, *(2,)))
print(g(*(1, 2), c=3))

def h(a, *r):
    return a + len(r)
print(h(*(1, 2, 3)))

def j(a, *r, **kw):
    return a + len(r) + 10 * len(kw)
print(j(1, *(2, 3)), j(*(1, 2)), j(*(1, 2), z=0))

def k(a, b, *, c=0):
    return a + b + c
print(k(*(1, 2), c=5))

def s(a, b):
    return str(a) + ":" + str(b)
print(s(*(1, "x")))
for pr in [(1, 2), (3, 4)]:
    print(s(*pr))

class P:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def m(self, a, b):
        return self.x * a + self.y * b

p = P(*(3, 4))
print(p.x, p.y, p.m(*(1, 2)))
