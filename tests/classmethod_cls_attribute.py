# issues/184: attributes read through `cls` in a @classmethod.
class C:
    base = 10
    @classmethod
    def h(cls):
        return 3
    @classmethod
    def g(cls, n):
        return cls.base + cls.h() + n
    @classmethod
    def name(cls):
        return cls.__name__

class D(C):
    base = 20

print(C.g(1))
print(D.g(1))          # cls is D: its own base
print(C().g(2))        # through an instance
print(C.name(), D.name())
