from pyc_compat import pyc_struct

@pyc_struct
class Point:
  def __init__(self, x, y):
    self.x = x
    self.y = y
  def magnitude_squared(self):
    return self.x * self.x + self.y * self.y

# Wrap construction in a function so the Point instance never
# touches a global slot.  Issue 023 Stage 2: the sret-returning
# constructor's slot should alloca in `compute`, not GC_malloc.
def compute(x, y):
  p = Point(x, y)
  return p.magnitude_squared()

print(compute(3, 4))
print(compute(5, 12))
print(compute(8, 15))
