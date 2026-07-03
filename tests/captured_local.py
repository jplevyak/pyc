def make_adder(n):
  return lambda x: x + n
f = make_adder(3)
g = make_adder(10)
print(f(4)); print(g(4)); print(f(100))
