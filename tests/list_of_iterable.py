# `list(x)` on anything that is iterable but is not one of the ten builtin
# classes that define __pyc_tolist__.
#
# python_ifa_build_if1.cc's list() intercept dispatches __pyc_tolist__
# directly on the argument with no fallback to the iterable protocol, so a
# user class -- or collections.defaultdict -- produced nothing from list()
# and the failure surfaced downstream as `unresolved call '__iter__'` on
# the bottom result (shedskin_examples life's `for pos in list(board):`).
# object.__pyc_tolist__ is the fallback.

from collections import defaultdict

# a plain user class that is iterable by delegating __iter__
class Bag:
    def __init__(self, items):
        self.items = items
    def __iter__(self):
        return iter(self.items)

b = Bag([3, 1, 2])
print(list(b))
print(len(list(b)))
print(sorted(list(b)))

# listing it twice must give the same thing -- a re-iterable container is
# not consumed by list()
print(list(b))

# a user class implementing CPython's iterator protocol directly
class Countdown:
    def __init__(self, n):
        self.n = n
    def __iter__(self):
        return self
    def __next__(self):
        if self.n <= 0:
            raise StopIteration
        self.n = self.n - 1
        return self.n

print(list(Countdown(4)))

# collections.defaultdict -- life's case
d = defaultdict(int)
d[1] = 10
d[2] = 20
ks = list(d)
print(sorted(ks))
print(len(ks))

# and iterating the result of list() on it, which is what life does
tot = 0
for k in list(d):
    tot = tot + d[k]
print(tot)
