class Counter:
  v = 0
  get = lambda y: y.v

def call_with(f):
  print(f())

c = Counter()
c.v = 42
call_with(c.get)
