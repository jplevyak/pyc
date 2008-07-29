# demonstrates flow dependent scoping
z = 1
def y():
  i = 0
  while i < 2: 
    if i == 0:
      global z 
    else:
      z = 2
    i = i + 1
  print z
y()
print z
