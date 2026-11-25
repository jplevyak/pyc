# functools.partial with positional arguments: stored ones first, then the
# call's, into named parameters, defaults, *args, methods, lambdas and
# nested partials. Keyword arguments are refused (functools_partial_keywords_refused).
import functools
from functools import partial


def power(base, exp):
    return base ** exp


def scale(k, x):
    return k * x


def join(sep, *parts):
    return sep.join(parts)


def greet(name, punct="!"):
    return "hi " + name + punct


cube_of = partial(power, 3)
print(cube_of(2), partial(power)(2, 5), partial(power, 2, 10)())
print(cube_of.func(2, 2), cube_of.args, partial(power, 1.5).args, len(cube_of.keywords))
print(list(map(partial(scale, 2), [1, 2, 3])))
print(sorted([3, -4, 1], key=partial(scale, -1)))
print(partial(join, "-", "a")("b", "c"), partial(join, "+")())
print(partial(greet, "bob")(), partial(greet, "ann")("?"), partial(greet, "x", ".")())
print(functools.partial(scale, 2.5)(4))


class Acc:
    def __init__(self):
        self.total = 0

    def add(self, a, b):
        self.total += a * b
        return self.total


acc = Acc()
f = partial(acc.add, 10)
f(1)
f(2)
print(acc.total)
print(partial(lambda a, b, c: a + b + c, "x")("y", "z"))
print(partial(partial(scale, 3))(4), partial(partial(power, 2), 3)())
fs = [partial(scale, i) for i in range(3)]
print([h(5) for h in fs])
