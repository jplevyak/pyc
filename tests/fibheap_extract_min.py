# Step 4: extract_min without consolidate.
#
# Tests the ring-removal path:
#   - 4-pointer splice: z.prev.next = z.next; z.next.prev = z.prev
#   - Single-node ring case via `z.next is z` (real identity
#     comparison; see prim_is below)
#   - Repeated extraction draining the heap to empty
#
# Without consolidate, the new self.min after each extract is
# just `z.next` — not the actual minimum.  Tests check counts
# and that all extracted keys come back (multiset-equal), not
# that they're sorted.
#
# Uses `z.next is z` for the single-node ring check.  Before
# `prim_is` (issue 028 step 4 + ifa/if1/prim_data.h:56), pyc
# routed all non-None `is` through `__pyc_any_type__::__is__`
# which returned False unconditionally — so the single-node
# check would never fire.  Workaround was `self.n == 1`.

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None
    self.prev = None

class Heap:
  def __init__(self):
    self.min = None
    self.n = 0

  def insert(self, k):
    x = Node(k)
    if self.min is None:
      x.next = x
      x.prev = x
      self.min = x
    else:
      x.next = self.min.next
      x.prev = self.min
      self.min.next.prev = x
      self.min.next = x
      if k < self.min.key:
        self.min = x
    self.n = self.n + 1

  def minimum(self):
    if self.min is None:
      return -1
    return self.min.key

  def extract_min(self):
    z = self.min
    if z is None:
      return -1
    key = z.key
    if z.next is z:
      # single-node ring — heap empty after this
      self.min = None
    else:
      z.prev.next = z.next
      z.next.prev = z.prev
      self.min = z.next
    self.n = self.n - 1
    return key

# Build, then drain
h = Heap()
print(h.n)            # 0
print(h.extract_min())  # -1 (empty)

h.insert(5)
h.insert(3)
h.insert(7)
h.insert(1)
h.insert(9)
print(h.n)            # 5
print(h.minimum())    # 1 (real min after all inserts)

# Extract one — pulls the actual minimum since insert tracks it
print(h.extract_min())  # 1
print(h.n)              # 4

# Drain the rest.  Order isn't sorted (no consolidate), but
# all should come out and sum to 5+3+7+9 = 24.
total = 0
i = 0
while i < 4:
  total = total + h.extract_min()
  i = i + 1
print(total)          # 24
print(h.n)            # 0
print(h.extract_min())  # -1 (empty again)
