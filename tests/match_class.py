class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

class Circle:
    def __init__(self, r):
        self.r = r

def classify(val):
    match val:
        case Point(x=0, y=0):
            print("origin")
        case Point(x=0, y=y):
            print("on y-axis at", y)
        case Point(x=x, y=y) if x == y:
            print("diagonal:", x)
        case Point(x=x, y=y):
            print("point:", x, y)
        case Circle(r=r):
            print("circle r=", r)
        case other:
            print("other:", other)

classify(Point(0, 0))
classify(Point(0, 5))
classify(Point(3, 3))
classify(Point(3, 4))
classify(Circle(7))
classify(42)
