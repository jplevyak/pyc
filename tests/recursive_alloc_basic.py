# Recursion with allocation — basic case.
#
# Each recursive call allocates a fresh Pair.  Verifies:
#   - IFA terminates on the recursive call graph.
#   - Per-call allocation works through both backends.
#   - Cross-CPython verify passes.
#
# Pair isn't a @pyc_struct, so each allocation goes through
# GC_malloc — this test is the "everything escapes" baseline.

class Pair:
  def __init__(self, a, b):
    self.a = a
    self.b = b

def sum_pairs(n):
  if n == 0:
    return 0
  p = Pair(n, n + 1)
  return p.a + p.b + sum_pairs(n - 1)

# n=5: (5+6) + (4+5) + (3+4) + (2+3) + (1+2) = 11+9+7+5+3 = 35
print(sum_pairs(5))
# n=10: 11+13+15+17+19+9+7+5+3+ wait let me recompute
# n=10: (10+11)+(9+10)+(8+9)+(7+8)+(6+7)+(5+6)+(4+5)+(3+4)+(2+3)+(1+2)
#     = 21+19+17+15+13+11+9+7+5+3 = 120
print(sum_pairs(10))
