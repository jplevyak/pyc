# Recursion with a local @pyc_struct that never escapes.
#
# The interesting case for escape analysis: each recursive
# call allocates a Vec2 locally, uses its fields to compute
# a scalar result, and never lets the Vec2 leave the frame
# (the recursion only passes the scalar return up).
#
# With ESCAPE_PLAN Phase 6 active, IFA should prove the
# Vec2 doesn't escape and emit `alloca %Vec2` instead of
# `call @GC_malloc` in mag_sq's body.  The C backend gets
# a GC heap allocation (matches CPython behavior); the v2
# LLVM backend should see the alloca optimization.
#
# This test passes if the math is correct on both backends.
# A separate manual probe (PYC_DUMP_LL=...) confirms that
# the IR contains `alloca %Vec2.*` — see ESCAPE_PLAN.md.

from pyc_compat import pyc_struct

@pyc_struct
class Vec2:
  def __init__(self, x, y):
    self.x = x
    self.y = y

def mag_sq(n):
  if n == 0:
    return 0
  v = Vec2(n, n + 1)
  return v.x * v.x + v.y * v.y + mag_sq(n - 1)

# n=5: 5²+6²=61, 4²+5²=41, 3²+4²=25, 2²+3²=13, 1²+2²=5
#      → 61+41+25+13+5 = 145
print(mag_sq(5))
# n=8: 8²+9²=145, 7²+8²=113, 6²+7²=85, 5²+6²=61, 4²+5²=41,
#      3²+4²=25, 2²+3²=13, 1²+2²=5
#      → 145+113+85+61+41+25+13+5 = 488
print(mag_sq(8))
