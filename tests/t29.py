class A:
  i = 3
  x = lambda y: y.i

a = A()
z = a.x
a.i = 4
print z()
