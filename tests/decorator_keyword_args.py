# A decorator call with keyword arguments, `@mk(n=2)`, passes them by name;
# they were added as positionals with no value and the compiler asserted
# (issues/207).
class D:
    def __init__(self, n, label="x"):
        self.n = n
        self.label = label
    def __call__(self, f):
        self.f = f
        return self
def mk(n=1, label="d"):
    return D(n, label)
@mk(n=2, label="two")
def g(x):
    return x
@mk(3)
def h(x):
    return x
print(g.n, g.label, g.f(5), h.n, h.label)
