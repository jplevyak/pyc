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
print B.n
A.n = 4
print A.n
print B.n
print A().n
print B().n
B.n = 5
print A.n
y = B();
print y.n
print y.o
