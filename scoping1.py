y = 2
z = 1

def set_z():
  global z
  z = 3

class a:
  print z
  if y == 2:
    set_z()
  print z
  if y == 2:
    z = 2
  print z

print z
