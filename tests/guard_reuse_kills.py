# ifa/177: where a guard's load of a module variable must NOT be reused in
# its branch -- a store or del in the branch (including one late in a loop),
# a function that declares the variable global and is called, a load under
# 'or' -- plus the elif path. Each must print what CPython prints.
class A:
    def __init__(self): self.f = 0
    def name(self): return 'A'
class B:
    def __init__(self): self.g = 0
    def name(self): return 'B'

def rebind():
    global y
    y = B()

xs = [A(), B()]
x = xs[0]
if isinstance(x, A):
    x.f = 4
print(xs[0].f, xs[1].g)

# a store in the branch: the later read must see the new value
z = xs[0]
if isinstance(z, A):
    z.f = 5
    z = xs[1]
    z.g = 6
print(xs[0].f, xs[1].g)

# a loop in the branch that rebinds: the next iteration reads the new value
w = xs[0]
n = 0
if w is not None:
    while n < 2:
        print(w.name())
        w = xs[1]
        n += 1

# a function declares the variable global, and the branch calls it
y = xs[0]
if isinstance(y, A):
    rebind()
    print(y.name())

# the load is under `or`: not reused
v = xs[1]
u = None
if u is None or isinstance(v, A):
    print(v.name())

# elif
q = xs[1]
if isinstance(q, A):
    print('A', q.f)
elif isinstance(q, B):
    print('B', q.g)

# del in the branch
d = xs[0]
if d is not None:
    del d
    d = xs[1]
    print(d.name())
