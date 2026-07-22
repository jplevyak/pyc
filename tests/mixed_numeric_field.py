# issue 025 (fysphun): a class field initialized int (`self.x = 0`) but
# later assigned float is a `int64 | float64` mix. Without numeric
# coercion of the field's ivar the struct field has no single C type
# and degrades to void*, breaking arithmetic on it. FA now coerces
# pure-numeric user record fields to the widest member (float), the
# same shedskin-style unification already applied to local variables.
class Point:
    def __init__(self):
        self.x = 0      # int in __init__
        self.y = 0

def midpoint(a, b):
    return (a.x + b.x) / 2.0, (a.y + b.y) / 2.0

a = Point()
a.x = 3.0               # float assigned -> Point.x is int|float
a.y = 4.0
b = Point()
b.x = 1.0
b.y = 2.0
mx, my = midpoint(a, b)
print(mx)
print(my)
print(a.x - b.x)

# in-place accumulation on a mixed field (fysphun's `p.x += ...`)
c = Point()
c.x = 0                 # stays int until...
for v in [1.5, 2.5, 3.0]:
    c.x += v            # ...float accumulation
print(c.x)
