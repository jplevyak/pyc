class A:
  n = 0
  def __init__(self, x):
    self.n = x 
  def __add__(self, x):
    return self.n + x.n

a = A(3)
print a + a
