# issues/023: positional class patterns (`case Point(0, 0):`), matched
# via a compile-time read-back of the class body's `__match_args__`
# literal -- previously unsupported (kept-only-keyword-form fail()).
class Point:
    __match_args__ = ("x", "y")

    def __init__(self, x, y):
        self.x = x
        self.y = y

def classify(val):
    match val:
        case Point(0, 0):
            print("origin")
        case Point(0, y):
            print("on y-axis at", y)
        case Point(x, y) if x == y:
            print("diagonal:", x)
        case Point(x, y):
            print("point:", x, y)
        case other:
            print("other:", other)

classify(Point(0, 0))
classify(Point(0, 5))
classify(Point(3, 3))
classify(Point(3, 4))
classify(42)

# Mixed positional + keyword in one pattern.
def classify2(val):
    match val:
        case Point(0, y=5):
            print("special row")
        case Point(x, y=0):
            print("on x-axis at", x)
        case Point(x, y):
            print("point:", x, y)

classify2(Point(0, 5))
classify2(Point(9, 0))
classify2(Point(1, 1))

# A subclass with NO __match_args__ of its own inherits the base
# class's -- positional patterns against it must still resolve
# position -> attribute name via Point's declaration.
class Point3D(Point):
    def __init__(self, x, y, z):
        Point.__init__(self, x, y)
        self.z = z

def classify3(val):
    match val:
        case Point3D(0, 0):
            print("3d, x=y=0, z=", val.z)
        case Point3D(x, y):
            print("3d:", x, y, val.z)

classify3(Point3D(0, 0, 9))
classify3(Point3D(1, 2, 3))

# A positional capture must bind a FRESH local, not reuse/mutate a
# same-named outer variable -- caught a real bug here: positional
# class-pattern args weren't going through mark_pattern_captures, so
# `x`/`y` resolved as ordinary reads of the module globals below and
# the match's binding silently clobbered them instead of shadowing.
x = 999
y = 888

def classify4(val):
    match val:
        case Point(x, y):
            print("bound:", x, y)

classify4(Point(3, 4))
print("outer still:", x, y)
