# test redefinition as global

z = 3
def y():
  z = 1;
  print z
  def g():
    x = 2
    print x
  global z
  g()
y()
print z

