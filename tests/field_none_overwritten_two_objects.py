# ifa/191: two objects of one class, each overwritten before any read, in
# the opposite order from construction. The None stays out of both.
class A:
    def __init__(self):
        self.x = None

a = A()
b = A()
b.x = 5
a.x = 7
print(a.x + b.x)
