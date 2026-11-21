# issues/195: @dataclass. Fields in order, defaults evaluated once,
# default_factory called per instance, ClassVar not a field, field(repr=False),
# __repr__ / __eq__ / unhashable __hash__ as CPython generates them, and a
# method the class defines itself kept.
from dataclasses import dataclass, field
from typing import List, Set, ClassVar


class Unit:
    def __repr__(self):
        return "Unit()"


@dataclass
class Point:
    x: int
    y: int = 0


@dataclass
class Bag:
    name: str
    items: List[int] = field(default_factory=list)
    tags: Set[str] = field(default_factory=set)
    count: ClassVar[int] = 0
    hidden: int = field(default=7, repr=False)
    unit: Unit = Unit()                       # one object: the class attribute IS the default

    def total(self):
        return sum(self.items)


@dataclass
class Named:
    label: str

    def __repr__(self):
        return "<" + self.label + ">"


p = Point(1)
q = Point(1, 2)
print(p, q, p == Point(1, 0), p == q, p != q, p.y, Point.y)
print(Point(y=5, x=4), Point(3, y=1))
a = Bag("a")
b = Bag("b")
a.items.append(3)
a.items.append(4)
a.tags.add("t")
b.tags.add("u")
print(a, b, a.total(), Bag.count, a.hidden)
print(a.unit is b.unit, a.unit is Bag.unit, Bag("c", [1]) == Bag("c", [1]), Bag("c", [1]) == Bag("c", [2]))
print(Named("n"), Named("n") == Named("n"), p == 3)
try:
    hash(p)
except TypeError as e:
    print("TypeError", e)
