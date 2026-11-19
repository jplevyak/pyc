# issues/182: keyword-only, *args and **kw parameters on generators and
# lambdas, through a forwarding decorator, and a default before *args.
def gen(n, *, step=1):
    i = 0
    while i < n:
        yield i
        i += step
print(list(gen(5)), list(gen(6, step=2)))

def gs(*xs, **kw):
    for x in xs:
        yield x + len(kw)
print(list(gs(1, 2, a=0)))

h = lambda a, *r, k=2: a + len(r) + k
print(h(1), h(1, 2, 3), h(1, k=10))

def deco(fn):
    def inner(*a, **kw):
        return fn(*a, **kw) + 1
    return inner

@deco
def total(*xs, **kw):
    return sum(xs) + len(kw)
print(total(1), total(1, 2), total(1, y=5, z=0))

def add(x, y=10, **kw):
    return x + y + len(kw)
print(add(1), add(1, 2), add(1, y=5, z=0))

def d2(a, b=2, *args, k=3):
    return (a, b, args, k)
print(d2(1), d2(1, 5), d2(1, 5, 6, 7), d2(1, k=9))
