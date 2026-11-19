# issues/172 #1: the value of `x = yield` is typed per generator, through a
# cell on the generator object that send() and next() write. It used to
# travel the coroutine as an int64: a resume by next() read 0 for CPython's
# None, and a send() of anything but an int was refused.

def g():
    x = yield 1
    print(x)                  # a resume by next(): None
    yield 2

it = g()
print(next(it))
print(next(it))

class P:
    def __str__(self):
        return "P!"

def echo():
    while True:
        x = yield "ready"
        print("got", x)

e = echo()
print(next(e))                # the priming next() delivers nothing
print(e.send("a"))
print(e.send("bc"))

def objs():
    x = yield 0
    print(x)
    yield 1

o = objs()
next(o)
o.send(P())

def pairs():
    t = yield 0
    print(t[0] + t[1])
    yield 1

pp = pairs()
next(pp)
pp.send((3, 4))

# A str sent into an outer generator reaches the inner one's yield.
def inner():
    x = yield "start"
    return x + "!"

def outer():
    r = yield from inner()
    yield r

d = outer()
print(next(d))
print(d.send("hi"))
