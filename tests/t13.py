z = 1
class A:
  x = 2
  print x
  def __init__(self):
    self.x = self.x + 1
  def f(self):
    print self.x
    return self.x + 1
y = A();
y.__init__()
print y.f()
