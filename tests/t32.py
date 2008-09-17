class A:
  n = 0
  def __init__(self, x):
    self.n = x 
  def __iadd__(self, x):
    self.n += x
    return self

x = 1
x += 2
print x
a = A(1)
print a.n
a += 2
print a.n

