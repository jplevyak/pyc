# issues/181: a DECORATED generator -- @staticmethod, @classmethod, or any
# decorator -- gets the wrapper that makes its generator object, as an
# undecorated one does. The decorated path built none, so the call returned
# the raw coroutine handle and `for v in C.s():` found no __iter__.
class C:
    def __init__(self, v):
        self.v = v

    @staticmethod
    def s():
        yield 3
        yield 4

    @staticmethod
    def count(n, step):
        i = 0
        while i < n:
            yield i
            i += step

    @staticmethod
    def echo():
        x = yield "ready"
        yield "got " + x

    @staticmethod
    def ret():
        yield 1
        return "done"

    @classmethod
    def make(cls, n):
        for i in range(n):
            yield cls(i)

for v in C.s():
    print(v)
print(list(C.count(5, 2)))
print(list(C(0).count(3, 1)))          # through an instance
e = C.echo()
print(next(e))
print(e.send("hi"))                    # send() reaches the static generator

def outer():
    r = yield from C.ret()
    yield 100
    print(r)                           # its return value
print(list(outer()))
print([c.v for c in C.make(3)])

# Any decorator: it receives the generator function, as in CPython.
def twice(f):
    def w(n):
        for v in f(n):
            yield v
            yield v
    return w

@twice
def up(n):
    for i in range(n):
        yield i
print(list(up(2)))

def ident(f):
    return f

# A decorated generator that captures an enclosing local.
def make(k):
    @ident
    def g():
        x = yield k
        yield x + k
    return g

it = make("a")()
print(next(it))
print(it.send("b"))
