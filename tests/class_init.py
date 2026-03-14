z = 1
class A:
    x = 2
    print(x)
    def __init__(self):
        self.x = self.x + 1
    def f(self):
        print(self.x)
        return self.x + 1

# normal instantiation
y = A()
print(y.f())

# explicit __init__ call
y2 = A()
y2.__init__()
print(y2.f())
