# issues/184: attributes read through `cls` in a @classmethod.
class C:
    base = 10
    @classmethod
    def h(cls):
        return 3
    @classmethod
    def g(cls, n):
        return cls.base + cls.h() + n

class D(C):
    base = 20

# Through `cls`, a plain method is the unbound function and a
# staticmethod takes no receiver, as in CPython.
class E:
    def f(self, x):
        return x + 1
    @staticmethod
    def s(x):
        return x * 2
    @classmethod
    def k(cls, y):
        return cls.f(cls(), y) + cls.s(y)

print(C.g(1))
print(D.g(1))          # cls is D: its own base
print(C().g(2))        # through an instance
print(E.k(5))
