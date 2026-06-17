# Two lambdas bound from the same instance.  Exercises the
# closure struct dispatch — without the post-F.4.8 real closure
# handler, both `f` and `g` would alias the same MOVE-bound-obj
# slot and the FA-resolved targets would collide.

class C:
  v = 0
  inc = lambda y: y.v + 1
  dec = lambda y: y.v - 1

c = C()
c.v = 10
f = c.inc
g = c.dec
print(f())
print(g())
