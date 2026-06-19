from pyc_compat import pyc_struct

@pyc_struct
class Point:
  def __init__(self, x, y):
    self.x = x
    self.y = y
  def magnitude_squared(self):
    return self.x * self.x + self.y * self.y

p = Point(3, 4)
print(p.x)
print(p.y)
print(p.magnitude_squared())

q = Point(-2, 5)
print(q.x)
print(q.y)
print(q.magnitude_squared())
