class Point:
  def __init__(self, x, y):
    self.x = x
    self.y = y
  def __str__(self):
    return "Point(" + str(self.x) + ", " + str(self.y) + ")"

x = 5
print(str(x))
print(str(3.14))
print(str("hi"))
print(str(True))
print(str(False))
p = Point(1, 2)
print(str(p))
