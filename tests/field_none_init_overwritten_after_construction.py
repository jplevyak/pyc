# PASSES since ifa/191 (2026-10-17): the None is never read before the
# overwrite, so it is not in the field's type. The text below describes
# the failure this test was written for.
# ifa/191: the None initial value is overwritten by the caller after
# construction, before any read. CPython prints 6.
class A:
    def __init__(self):
        self.x = None

a = A()
a.x = 5
print(a.x + 1)
