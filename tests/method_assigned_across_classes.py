# issues/180: a bound method of one class stored into an instance of another
# is called as the stored value. pyc called through the closure struct as if
# it were the class's method slot, and segfaulted.
class A:
    def x(self):
        return 1

class B:
    def x(self):
        return 2

a = A()
b = B()
a.x = b.x
print(a.x())
