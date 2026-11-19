# issues/182: `f(*t)` and `f(**d)` hand the callee its whole *args tuple and
# **kw dict: a list unpacks to a tuple, the dict is copied, and both
# forward through a wrapper and to a base-class method.
def f(*a):
    return len(a)
t = (1, 2)
print(f(*t))
print(f(*[4, 5, 6]))

def show(*a):
    print(a)
show(*[7, 8])
show(*(1, "x"))

def k(**kw):
    return sorted(kw.keys())
d = {"b": 1, "a": 2}
print(k(**d))

def inner(x, *args, **kw):
    return x + len(args) + 10 * len(kw)
def outer(x, *args, **kw):
    return inner(x, *args, **kw)
print(outer(1), outer(1, 2, 3), outer(1, 2, p=1, q=2))

def mut(**kw):
    kw["z"] = 0
    return len(kw)
print(mut(**d), len(d))

class B:
    def __init__(self, *args, **kw):
        self.n = len(args) + len(kw)
class D(B):
    def __init__(self, *args, **kw):
        B.__init__(self, *args, **kw)
print(D(1, 2, a=3).n)
