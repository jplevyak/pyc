# pyc_lib/itertools.py's `count` shim defined a Python-2-style
# `next(self)` method instead of Python 3's `__next__` -- every other
# iterator class in __pyc__/ uses __next__, so pyc's next() builtin
# never found a match and count() aborted at runtime with "getter not
# resolved". Found while investigating issues/025's TODO list item 13.

from itertools import count, product

c = count(5, 2)
for _ in range(4):
    print(next(c))

for p in product([1, 2], [3, 4]):
    print(p)

for p in product([1, 2], [3, 4], [5, 6]):
    print(p)
