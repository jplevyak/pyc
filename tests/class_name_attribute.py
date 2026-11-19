# issues/190: a class's __name__, by name and through `cls`.
class C:
    @classmethod
    def name(cls):
        return cls.__name__

class D(C):
    pass

print(C.__name__)
print(C.name(), D.name())
