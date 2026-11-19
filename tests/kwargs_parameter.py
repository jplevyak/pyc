# issues/182: a **kwargs parameter collects the keyword arguments no formal
# is named for, alone and beside *args, keyword-only parameters and
# defaults; in functions, methods and constructors.
def f(**kw):
    return kw["x"] + len(kw)
print(f(x=3, y=4))

def g(a, *args, k=1, **kw):
    return a + len(args) * 10 + k * 100 + len(kw) * 1000
print(g(1))
print(g(1, 2, 3))
print(g(1, 2, k=5))
print(g(1, k=5, z=9, w=8))
print(g(a=7, q=1))

def names(**kw):
    ks = sorted(kw.keys())
    return ",".join(ks)
print(names(b="x", a="y"))
print(names())

class C:
    def __init__(self, name, **opts):
        self.name = name
        self.opts = opts
    def m(self, *xs, sep="-", **kw):
        return sep.join([str(x) for x in xs]) + str(len(kw))
c = C("n", depth=3, width=4)
print(c.name, c.opts["depth"] + c.opts["width"])
print(c.m(1, 2, 3), c.m(1, 2, sep="+"), c.m(sep="/", extra=1))
