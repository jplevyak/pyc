# Recursion with multiple allocations per call.
#
# A doubly-recursive (fib-shape) function that allocates two
# Pair objects per invocation and uses them to thread args
# into the two recursive calls.  Stresses IFA's handling of:
#   - Allocation in a function reached via recursive call.
#   - Multiple fresh allocations within a single frame.
#   - Branching recursion (each level produces 2 children).
#
# Pair isn't a @pyc_struct, so the allocations go through
# GC_malloc.  This test sits alongside recursive_alloc_basic
# as the "many allocations escape per call" stress.

class Pair:
  def __init__(self, lo, hi):
    self.lo = lo
    self.hi = hi

def range_sum(lo, hi):
  if lo >= hi:
    return 0
  if hi - lo == 1:
    return lo
  mid = (lo + hi) // 2
  left = Pair(lo, mid)
  right = Pair(mid, hi)
  return range_sum(left.lo, left.hi) + range_sum(right.lo, right.hi)

print(range_sum(0, 6))    # 0+1+2+3+4+5 = 15
print(range_sum(1, 8))    # 1+2+3+4+5+6+7 = 28
print(range_sum(0, 16))   # 0..15 = 120
