# test use global in function before define

class a:
  global z
  print z

a();

z = 1

print z
