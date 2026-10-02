# ifa/178: an attribute read on a receiver that can only be None is
# CPython's AttributeError at run time, not a compile error. Here it sits on
# a path that never executes (`flag` is always false, but not to FA), so CPython prints
# 1 and 2. The None-only contour of `get` never returns, and the code after
# `get(None)` in `pick` is unreachable rather than untyped.
class A:
  def __init__(self, v):
    self.v = v

def get(o):
  return o.v

def pick(o, flag):
  if flag:
    r = get(None)
    return r + 1
  return get(o)

flags = []
flags.append(0)
off = len(flags) > 3
print(pick(A(1), off))
print(pick(A(2), off))
