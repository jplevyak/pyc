# issues/193: zero-argument super() is super(__class__, <first argument>).
# It lowered the receiver from the function's closure self, null in an
# ordinary method, and asserted. The class is the one the method is
# DEFINED in: B.f's super() reaches A even when called on a C.


class A:
    def __init__(self, x):
        self.x = x

    def f(self):
        return "A" + str(self.x)


class B(A):
    def __init__(self, x, y):
        super().__init__(x)
        self.y = y

    def f(self):
        return "B" + super().f()


class C(B):
    def __init__(self):
        super().__init__(1, 2)

    def f(self):
        return "C" + super().f()

    def other(this):
        return super().f()        # the first argument need not be `self`


c = C()
print(c.f(), c.x, c.y)
print(B(5, 6).f())
print(c.other())
