class Counter:
  v = 0
  get = lambda y: y.v

def make():
  c = Counter()
  c.v = 42
  return c.get

def use(f):
  print(f())

use(make())
