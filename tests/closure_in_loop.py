# Closure bound once, called multiple times from inside a
# loop.  Exercises the new closure FIELD_LOAD dispatch from a
# SSU-phi-typed slot (the loop-carried closure rval), which is
# different from the straight-line call path lambda_closure.py
# covers.

class Acc:
  v = 0
  get = lambda y: y.v

a = Acc()
a.v = 7
f = a.get
total = 0
i = 0
while i < 5:
  total = total + f()
  i = i + 1
print(total)
