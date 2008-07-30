# define test 2 levels of locals

def f():
  z = 1;
  print z
  def g():
    x = 2
    print x
  g()
  print z
f()

