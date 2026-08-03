# issues/014: `yield from EXPR` delegates to a sub-generator, forwarding
# yielded values out, .send() values in, and the sub-generator's return
# value back as the yield-from expression's own value. Desugars to a
# loop calling EXPR.send(...) and catching StopIteration explicitly
# (build_if1_pyda's PY_yield_from_expr case) -- delegation to a plain
# (non-generator) iterable is not supported (needs .send(), which only
# __pyc_generator__ instances provide).

def inner():
    yield 1
    yield 2

def outer():
    yield from inner()

for v in outer():
    print(v)

# The delegation expression's own value is the sub-generator's return
# value (StopIteration.value) once it's exhausted.
def inner_with_return():
    yield 10
    yield 20
    return 100

def outer_captures_result():
    result = yield from inner_with_return()
    yield result + 1

g = outer_captures_result()
print(g.__next__())
print(g.__next__())
print(g.__next__())

# .send() forwards bidirectionally through the delegation: values sent
# into the outer generator reach the inner one's paused `x = yield`
# expression, and the inner's yields reach the outer's own caller.
def echo_inner():
    x = yield 1
    y = yield x + 10
    return x + y

def echo_outer():
    total = yield from echo_inner()
    yield total * 1000

e = echo_outer()
print(e.__next__())
print(e.send(5))
print(e.send(7))

# An exception raised inside the delegated (inner) generator propagates
# out through the outer generator's yield-from, uncaught by it --
# exactly as if the inner generator's code ran inline in the outer one.
class Boom(Exception):
    value = 0
    def __init__(self, value=0):
        self.value = value

def raiser():
    yield 1
    raise Boom(77)

def delegates_to_raiser():
    yield from raiser()
    yield 999

g2 = delegates_to_raiser()
print(g2.__next__())
try:
    g2.__next__()
except Boom as exc:
    print(exc.value)
