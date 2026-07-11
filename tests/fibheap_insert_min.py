# Minimal fib-heap-style structure: Heap class with a
# circular doubly-linked root list of Nodes.
#
# Exercises:
#   - Class encapsulating a ring (self.min + Node.next/prev)
#   - `is None` check on a property, with both branches
#     modifying state and converging to shared cleanup
#     (self.n += 1) AFTER the if/else.
#   - SSU rename of `self` across an if/else merge
#     (issue 028 Bug B — fixed by iterating place_phi /
#     place_phy until both converge in ssu.cc).
#
# NB: `m = self.min` local binding is the workaround for
# issue 028 Bug A (`is None` narrowing applies to locals,
# not to property re-reads — fresh `self.min` access in
# the else branch would lose the narrowing).  Bug A is
# not yet fixed; this binding is the documented pattern.

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
    m = self.min
    if m is None:
      x.next = x
      x.prev = x
      self.min = x
    else:
      x.next = m.next
      x.prev = m
      m.next.prev = x
      m.next = x
      if k < m.key:
        self.min = x
    self.n = self.n + 1

  def minimum(self):
    m = self.min
    if m is None:
      return -1
    return m.key

h = Heap()
print(h.n)             # 0
print(h.minimum())     # -1

h.insert(5)
print(h.n)             # 1
print(h.minimum())     # 5

h.insert(3)
print(h.n)             # 2
print(h.minimum())     # 3

h.insert(7)
print(h.n)             # 3
print(h.minimum())     # 3

h.insert(1)
print(h.n)             # 4
print(h.minimum())     # 1
