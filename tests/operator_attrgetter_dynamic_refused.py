# operator.attrgetter needs constant names in pyc, which resolves attributes
# at compile time (issues/216); a computed name is refused.
from operator import attrgetter


class P:
    def __init__(self):
        self.x = 1


name = "x"
print(attrgetter(name)(P()))
