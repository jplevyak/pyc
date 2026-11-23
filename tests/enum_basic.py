# issues/208: enum.Enum -- members, aliases, auto(), lookup, iteration.
from enum import Enum, auto, unique
import enum


class Color(Enum):
    RED = 1
    GREEN = 2
    BLUE = 3
    CRIMSON = 1  # an alias of RED

    def describe(self):
        return self.name.lower() + "=" + str(self.value)

    @classmethod
    def favorite(cls):
        return cls.GREEN


@unique
class Shape(enum.Enum):
    SQUARE = auto()
    CIRCLE = auto()
    TRIANGLE = 10
    HEXAGON = auto()


print(Color.RED, repr(Color.GREEN), Color.BLUE.name, Color.BLUE.value)
print(Color.CRIMSON is Color.RED, Color.CRIMSON.name)
print([c.name for c in Color], len(Color))
print(list(Color.__members__))
print(Color(2), Color["BLUE"], Color(1) is Color.RED)
print(Color.RED in Color, 3 in Color, 7 in Color)
print(Color.RED == Color.RED, Color.RED == Color.GREEN, Color.RED != Color.BLUE)
print(Color.GREEN.describe(), Color.favorite())
print(f"{Color.RED}", "%s" % Color.BLUE, str(Color.GREEN))
print([s.value for s in Shape], Shape.HEXAGON)
print(list(reversed(Color)))
d = {Color.RED: "r", Color.GREEN: "g"}
print(d[Color.CRIMSON], hash(Color.RED) == hash("RED"))
try:
    Color(9)
except ValueError as e:
    print("ValueError:", e)


def pick(c):
    if c is Color.RED:
        return "stop"
    elif c is Color.GREEN:
        return "go"
    return "?"


for c in Color:
    print(pick(c), end=" ")
print()
