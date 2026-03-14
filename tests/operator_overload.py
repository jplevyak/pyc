# __add__ operator overloading
class Addable:
    n = 0
    def __init__(self, x):
        self.n = x
    def __add__(self, x):
        return self.n + x.n

a = Addable(3)
print(a + a)

# __call__ callable object
class Callable(object):
    def __call__(self, x, y):
        return x + y

b = Callable()
print(b(1, 2))

# __iadd__ in-place operator
class InplaceAdd:
    n = 0
    def __init__(self, x):
        self.n = x
    def __iadd__(self, x):
        self.n += x
        return self

x = 1
x += 2
print(x)
c = InplaceAdd(1)
print(c.n)
c += 2
print(c.n)
