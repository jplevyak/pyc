# issues/171 #7: a generator's return value has its own channel. It used
# to share fn->ret with the yielded values and come back through an int:
# `yield "q"; return "s"` printed an address, `yield 1; return "s"` was an
# {int, str} that could not be typed, and a generator with no `return X`
# reported 0 where CPython reports None.
def a():
    yield 1
    return "s"

def b():
    yield 2
    return 7

def c():
    yield 3

def outer():
    x = yield from a()
    y = yield from b()
    z = yield from c()
    print(x, y, z)

for v in outer():
    print(v)

def q():
    yield "q"
    return "r"

g = q()
print(next(g))
for w in g:
    pass

def maybe(n):
    yield n
    if n > 0:
        return n * 10
    return -1

def use_maybe(n):
    r = yield from maybe(n)
    print("maybe", n, r)

for v in use_maybe(3):
    pass
for v in use_maybe(0):
    pass
