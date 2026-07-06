# issues/001: nested def capturing an enclosing local -- now fully
# clean (zero FA warnings) on both backends since issues/007's split
# identity removed the name-rebind warning that kept this shape out
# of the official suite.
def make_adder(n):
  def adder(x):
    return x + n
  return adder

f = make_adder(3)
g = make_adder(100)
print(f(4))
print(g(4))
print(f(99))
