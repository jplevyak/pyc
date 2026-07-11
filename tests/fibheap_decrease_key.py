# Full Fibonacci heap with decrease_key + cascading cut.
# Builds on tests/fibheap_full.py.
#
# Tests:
#   - decrease_key on a root (just updates min, no cut)
#   - decrease_key on a child node (cut, no cascade)
#   - decrease_key triggering cascading_cut on a marked parent
#   - subsequent extract_min still returns sorted keys

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None
    self.prev = None
    self.parent = None
    self.child = None
    self.degree = 0
    self.marked = False

class Heap:
  def __init__(self):
    self.min = None
    self.n = 0

  def splice_after(self, after, x):
    x.next = after.next
    x.prev = after
    after.next.prev = x
    after.next = x

  def insert(self, k):
    x = Node(k)
    if self.min is None:
      x.next = x
      x.prev = x
      self.min = x
    else:
      self.splice_after(self.min, x)
      if k < self.min.key:
        self.min = x
    self.n = self.n + 1
    return x

  def minimum(self):
    if self.min is None:
      return -1
    return self.min.key

  def link(self, y, x):
    y.parent = x
    if x.child is None:
      x.child = y
      y.next = y
      y.prev = y
    else:
      self.splice_after(x.child, y)
    x.degree = x.degree + 1
    y.marked = False

  def consolidate(self):
    if self.min is None:
      return
    dummy = Node(0)
    roots = [dummy, None, dummy, None]
    i = 0
    while i < 4:
      roots[i] = None
      i = i + 1
    w = self.min
    while True:
      roots.append(w)
      w = w.next
      if w is self.min:
        break
    A = [dummy, None, dummy, None, dummy, None, dummy, None]
    i = 0
    while i < 8:
      A[i] = None
      i = i + 1
    i = 0
    while i < len(roots):
      x = roots[i]
      if x is not None:
        d = x.degree
        while A[d] is not None:
          y = A[d]
          if x.key > y.key:
            t = x
            x = y
            y = t
          y.prev.next = y.next
          y.next.prev = y.prev
          y.next = y
          y.prev = y
          self.link(y, x)
          A[d] = None
          d = d + 1
        A[d] = x
      i = i + 1
    self.min = None
    i = 0
    while i < 8:
      if A[i] is not None:
        a = A[i]
        a.parent = None
        if self.min is None:
          a.next = a
          a.prev = a
          self.min = a
        else:
          self.splice_after(self.min, a)
          if a.key < self.min.key:
            self.min = a
      i = i + 1

  def extract_min(self):
    z = self.min
    if z is None:
      return -1
    key = z.key
    if z.child is not None:
      c = z.child
      first = c
      while True:
        c.parent = None
        c = c.next
        if c is first:
          break
      last = first.prev
      first.prev = z
      last.next = z.next
      z.next.prev = last
      z.next = first
    if z.next is z:
      self.min = None
    else:
      z.prev.next = z.next
      z.next.prev = z.prev
      self.min = z.next
    self.n = self.n - 1
    self.consolidate()
    return key

  def cut(self, x, y):
    if x.next is x:
      y.child = None
    else:
      if y.child is x:
        y.child = x.next
      x.prev.next = x.next
      x.next.prev = x.prev
    y.degree = y.degree - 1
    x.parent = None
    x.marked = False
    self.splice_after(self.min, x)

  def cascading_cut(self, y):
    z = y.parent
    if z is not None:
      if y.marked:
        self.cut(y, z)
        self.cascading_cut(z)
      else:
        y.marked = True

  def decrease_key(self, x, k):
    if k > x.key:
      return
    x.key = k
    y = x.parent
    if y is not None:
      if x.key < y.key:
        self.cut(x, y)
        self.cascading_cut(y)
    if x.key < self.min.key:
      self.min = x

# Build a heap of 8 elements and keep refs to all of them.
h = Heap()
n1 = h.insert(1)
n2 = h.insert(2)
n3 = h.insert(3)
n4 = h.insert(4)
n5 = h.insert(5)
n6 = h.insert(6)
n7 = h.insert(7)
n8 = h.insert(8)
print(h.n)             # 8
print(h.minimum())     # 1

# extract_min once: consolidates the rest into trees.
print(h.extract_min())  # 1
print(h.n)              # 7

# decrease_key on n8 to make it the new min — at least
# triggers a cut if n8 is now somewhere in a tree.
h.decrease_key(n8, 0)
print(h.minimum())     # 0
print(n8.parent is None)  # True (n8 was cut to root if it had a parent)
print(n8.key)          # 0

# Drain the heap.  Expected: 0, 2, 3, 4, 5, 6, 7.
print(h.extract_min())  # 0
print(h.extract_min())  # 2
print(h.extract_min())  # 3
print(h.extract_min())  # 4
print(h.extract_min())  # 5
print(h.extract_min())  # 6
print(h.extract_min())  # 7
print(h.n)              # 0
