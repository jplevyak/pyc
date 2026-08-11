# Regression (ifa/issues/087): keyword arguments given out of
# declaration order. `f(high=9, low=2)` against `def f(low, high)`
# previously matched fine through pattern.cc's Matcher but then hit
# Matcher::build()'s order_wrapper callback, which was an unimplemented
# IFACallbacks base-class stub -- silently dropping the match and
# surfacing downstream as a runtime "matching function not found" trap.
# Also covers reordering COMBINED with a skipped/defaulted argument
# (`g(c=3, a=1)` against `def g(a, b=99, c=100)`), which exposed a
# second bug: order_wrapper's forwarding send is itself re-matched by
# the compiler against the wrapped function's real signature, and
# without argument names attached that re-match falls back to raw
# position -- silently mismatching once reordering and default-skipping
# combine (b's default landed in c's slot and vice versa).
def f3(a, b, c):
    return (a, b, c)

print(f3(a=1, b=2, c=3))
print(f3(c=3, b=2, a=1))
print(f3(b=2, a=1, c=3))
print(f3(1, c=3, b=2))


class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def show(self):
        return (self.x, self.y)


p = Point(x=1, y=2)
print(p.show())
p2 = Point(y=20, x=10)
print(p2.show())


def g(a, b=99, c=100):
    return (a, b, c)


print(g(1))
print(g(a=1))
print(g(c=3, a=1))
print(g(1, c=3))
