# issues/195: a bare annotation (`x: int`) in a class body binds nothing in
# CPython. pyc made it a member with no value, and a subclass reading the
# field set by its own __init__ failed: "unresolved member 'x' of class 'A'".
class A:
    x: int
    y: int = 2

    def __init__(self, x):
        self.x = x


class B(A):
    def __init__(self, x, z):
        self.x = x
        self.z = z

    def show(self):
        return self.x + self.y + self.z


def f():
    n: int
    n = 4
    return n


print(A(1).x, B(1, 3).show(), A.y, f())
