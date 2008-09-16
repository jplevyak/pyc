class A(object):
  a = 1
  x = lambda y: y.a
class B(object):
  b = 2
  x = lambda y: y.b
a = A()
print a.x()
a.x = lambda y: y
print a.x(2)
b = B()
print b.x()
a.x = b.x
print a.x()
b.b = 3
print a.x()
