# issues/208: an enum with __init__ (tuple values), methods, _missing_,
# a memberless enum base with shared behaviour, and a match statement.
from enum import Enum, auto


class Planet(Enum):
    MERCURY = (3.303e23, 2.4397e6)
    EARTH = (5.976e24, 6.37814e6)

    def __init__(self, mass, radius):
        self.mass = mass
        self.radius = radius

    def surface_gravity(self):
        return 6.673e-11 * self.mass / (self.radius * self.radius)


class Ordered(Enum):
    def __lt__(self, other):
        return self.value < other.value


class Size(Ordered):
    SMALL = 1
    MEDIUM = 2
    LARGE = 3

    @classmethod
    def _missing_(cls, value):
        if value > 3:
            return cls.LARGE
        return None


class Mode(Enum):
    READ = "r"
    WRITE = "w"


print(Planet.EARTH.value, Planet.EARTH.mass)
print("%.2f %.2f" % (Planet.EARTH.surface_gravity(), Planet.MERCURY.surface_gravity()))
print(Size.SMALL < Size.LARGE, Size.LARGE < Size.MEDIUM, sorted([Size.LARGE, Size.SMALL, Size.MEDIUM]))
print(Size(10), Size(2))
try:
    Size(0)
except ValueError as e:
    print("ValueError:", e)
print(Mode("w"), [m.value for m in Mode], len(Mode), len(Size))


def describe(m):
    match m:
        case Mode.READ:
            return "reading"
        case Mode.WRITE:
            return "writing"
    return "?"


print(describe(Mode.READ), describe(Mode("w")))
