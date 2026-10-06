# ifa/issues/184: `x.m` on a receiver that may be None raises AttributeError
# at the attribute read, before the bound method is ever called. With the
# method stored first, the call is not a simple closure call, so no
# call-site check can see the receiver: pyc printed "got method" and 2.
import sys


class A:
    def m(self, k):
        return k + 1


x = None
if len(sys.argv) > 5:
    x = A()
print("before")
f = x.m
print("got method")
print(f(1))
