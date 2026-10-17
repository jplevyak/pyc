# ifa/191: the None initial value is overwritten by the caller after
# construction, before any read. CPython prints 6.
class A:
    def __init__(self):
        self.x = None

a = A()
a.x = 5
print(a.x + 1)
