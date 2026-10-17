# ifa/191: the None initial value is overwritten by a method __init__ calls,
# before any read. Interprocedural; CPython prints 6.
class A:
    def __init__(self):
        self.x = None
        self.setup()

    def setup(self):
        self.x = 5

a = A()
print(a.x + 1)
