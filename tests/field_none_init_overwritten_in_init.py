# ifa/191: a None initial value that __init__ itself overwrites before any
# read. The field's type is {None, int} because a field accumulates every
# write, so `a.x + 1` is refused (issues/183); CPython prints 6.
class A:
    def __init__(self):
        self.x = None
        self.x = 5

a = A()
print(a.x + 1)
