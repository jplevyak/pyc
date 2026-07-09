class Shape:
  tag = 0
class Circle(Shape):
  tag = 1
class Square(Shape):
  tag = 2
items = [Circle(), Square(), Circle()]
for it in items:
  print(it.tag)
