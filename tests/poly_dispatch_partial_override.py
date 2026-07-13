# ifa/issues 026: polymorphic method dispatch where one concrete
# class in the union relies purely on an inherited implementation
# rather than overriding the method itself (Square below).
class Shape:
    def describe(self):
        return "shape"

class Circle(Shape):
    def describe(self):
        return "circle"

class Square(Shape):
    pass

items = [Circle(), Square(), Shape()]
for it in items:
    print(it.describe())
