# Inheritance without super()
class Shape(object):
    n = 2
    print(n)
    def __init__(self, a):
        print(a + 10)
        print(self.n + 10)

class Circle(Shape):
    o = 3
    print(o)
    def __init__(self, a):
        print(a)
        print(self.n)
        print(self.o)

y = Circle(3)
print(y.n)
print(y.o)

# Inheritance with super()
class Vehicle(object):
    n = 2
    print(n)
    def __init__(self, a):
        print(a + 10)
        print(self.n + 10)

class Car(Vehicle):
    o = 3
    print(o)
    def __init__(self, a):
        super(Car, self).__init__(a)
        print(a)
        print(self.n)
        print(self.o)

y2 = Car(3)
print(y2.n)
print(y2.o)
z2 = Car(4)
