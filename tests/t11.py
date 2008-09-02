m = 1
class A:
  n = 2
  print m
  print n
print A.n
class B(A):
  o = 3
  print m
  print o
y = B();
print y.n
print y.o
