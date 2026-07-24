from pyc_compat import pyc_compare

# issue 068: the ordering family of the class-side derive.
# @pyc_compare derives __lt__ (lexicographic field fold) and the delegated
# __ne__/__gt__/__le__/__ge__; each field comparison is an ordinary send.


@pyc_compare
class P:
    def __init__(self, x, y):
        self.x = x
        self.y = y


a = P(1, 2)
b = P(1, 3)
c = P(1, 2)

print(a < b)   # True  (x equal, 2 < 3)
print(b < a)   # False
print(a < c)   # False (equal)
print(a > b)   # False
print(b > a)   # True
print(a <= c)  # True  (equal)
print(a >= c)  # True
print(a <= b)  # True
print(a >= b)  # False
print(a != b)  # True
print(a != c)  # False

# lexicographic: the first differing field decides
d = P(2, 0)
print(a < d)   # True  (1 < 2, second field ignored)
print(d < a)   # False
