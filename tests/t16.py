class A:
  x = 3
  print 1
  def __init__(self):
    print 5
print 4
print A.x
print 2
print A().x
