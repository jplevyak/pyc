# ifa/issues/184: a method call on a receiver that may be None, where the
# method never reads `self`, must raise as CPython does -- not print the
# method's result. `x.m(1)` is lowered as a call of the bound-method
# closure `x.m`; DCE ran before the inliner turned that into a direct
# send, so it never kept `x` live, and with `self` unused the receiver
# load was deleted and the ifa/165 check had nothing to test.
import sys


class A:
    def m(self, k):
        return k + 1


x = None
if len(sys.argv) > 5:
    x = A()
print("before")
print(x.m(1))
print("not reached")
