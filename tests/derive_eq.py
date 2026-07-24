from pyc_compat import pyc_compare

# issue 068: the class side of the derive / field-fold framework.
# @pyc_compare derives a field-wise __eq__; each field comparison is an
# ordinary send, so mixed field types dispatch to their own __eq__
# (int.__eq__ and str.__eq__ below).


@pyc_compare
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y


@pyc_compare
class Rec:
    def __init__(self, n, s):
        self.n = n
        self.s = s


a = Point(1, 2)
b = Point(1, 2)
c = Point(1, 3)
print(a == b)
print(a == c)

r1 = Rec(1, "hi")
r2 = Rec(1, "hi")
r3 = Rec(1, "bye")
print(r1 == r2)
print(r1 == r3)
