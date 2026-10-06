# ifa/issues/184: reading a field of a receiver that may be None must raise
# AttributeError at the read, as CPython does. `A.f` is always 3, so the
# read folded to its constant and nothing read `x` at all: pyc printed 3.
import sys


class A:
    def __init__(self):
        self.f = 3


x = None
if len(sys.argv) > 5:
    x = A()
print("before")
print(x.f)
print("not reached")
