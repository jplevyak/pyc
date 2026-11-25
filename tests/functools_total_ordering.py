# @total_ordering: each root (__lt__, __le__, __gt__, __ge__) generates the
# other three by CPython's formulas; an inherited root, a subclass, a class
# defining two roots (the greatest name wins), and sorting with the result.
import functools
from functools import total_ordering


@total_ordering
class Lt:
    def __init__(self, n):
        self.n = n

    def __eq__(self, other):
        return self.n == other.n

    def __lt__(self, other):
        return self.n < other.n


@total_ordering
class Le:
    def __init__(self, n):
        self.n = n

    def __eq__(self, other):
        return self.n == other.n

    def __le__(self, other):
        return self.n <= other.n


@functools.total_ordering
class Gt:
    def __init__(self, n):
        self.n = n

    def __eq__(self, other):
        return self.n == other.n

    def __gt__(self, other):
        return self.n > other.n


@total_ordering
class Ge:
    def __init__(self, n):
        self.n = n

    def __eq__(self, other):
        return self.n == other.n

    def __ge__(self, other):
        return self.n >= other.n


# __lt__ is the root (max of the names); __gt__ is its own, kept.
@total_ordering
class Two:
    def __init__(self, n):
        self.n = n

    def __eq__(self, other):
        return self.n == other.n

    def __lt__(self, other):
        return self.n < other.n

    def __gt__(self, other):
        return False


class Base:
    def __init__(self, n):
        self.n = n

    def __lt__(self, other):
        return self.n < other.n


@total_ordering
class Derived(Base):
    def __eq__(self, other):
        return self.n == other.n


class Sub(Lt):
    pass


for a, b in [(1, 2), (2, 2), (3, 2)]:
    print(Lt(a) < Lt(b), Lt(a) <= Lt(b), Lt(a) > Lt(b), Lt(a) >= Lt(b))
    print(Le(a) < Le(b), Le(a) <= Le(b), Le(a) > Le(b), Le(a) >= Le(b))
    print(Gt(a) < Gt(b), Gt(a) <= Gt(b), Gt(a) > Gt(b), Gt(a) >= Gt(b))
    print(Ge(a) < Ge(b), Ge(a) <= Ge(b), Ge(a) > Ge(b), Ge(a) >= Ge(b))
    print(Two(a) > Two(b), Two(a) >= Two(b), Derived(a) <= Derived(b), Derived(a) > Derived(b))
print(Sub(1) > Sub(0), sorted([Lt(3), Lt(1), Lt(2)])[0].n, max([Ge(3), Ge(5)]).n, min([Gt(4), Gt(2)]).n)
