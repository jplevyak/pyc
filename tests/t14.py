class A(object):
  n = 2
  print n
  def __init__(self, a):
    print a + 10
    print self.n + 10
class B(A):
  o = 3
  print o
  def __init__(self, a):
    # super(B, self).__init__(a)
    print a
    print self.n
    print self.o
y = B(3);
print y.n
print y.o
