# test 2 levels of global access

y = 2
z = 1

def set_z():
  global z
  z = 3

class a:
  def g():
    global z
    z = 5
  
  print z
  if y == 2:
    set_z()
  print z
  g();
  print z

print z
