# PASSES since ifa/191 (2026-10-17): the None is never read before the
# overwrite, so it is not in the field's type. The text below describes
# the failure this test was written for.
# ifa/191: a None initial value that __init__ itself overwrites before any
# read. The field's type is {None, int} because a field accumulates every
# write, so `a.x + 1` is refused (issues/183); CPython prints 6.
class A:
    def __init__(self):
        self.x = None
        self.x = 5

a = A()
print(a.x + 1)
