# Real identity comparison via `is` / `is not` on non-None
# operands.  Before prim_is (issue 028 step 4), pyc routed
# all non-None `is` through the `__pyc_any_type__::__is__`
# method, which returned False unconditionally — so
# `a is a` returned False, breaking ring-detection idioms.
#
# After: `is`/`is not` lower to prim_is, which emits
# pointer equality at codegen.

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None

# Same-object identity
a = Node(1)
print(a is a)              # True
print(a is not a)          # False

# Different objects, same fields → not identical
b = Node(1)
print(a is b)              # False
print(a is not b)          # True

# Aliased identity
c = a
print(c is a)              # True
print(c is not a)          # False

# Single-node ring detection (the motivating case)
a.next = a
print(a.next is a)         # True

# Multi-node ring — next is not self
b.next = a
print(b.next is b)         # False
print(b.next is a)         # True

# `is None` still works (separate prim_isinstance path)
d = None
print(d is None)           # True
print(a is None)           # False
print(a is not None)       # True
