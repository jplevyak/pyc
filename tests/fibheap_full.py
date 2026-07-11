# Full Fibonacci heap with consolidate (issue 028 step 5).
# Repeated extract_min yields keys in sorted order.
#
# Exercises every piece built up across issues 026 and 028:
#   - circular doubly-linked root list (steps 1-2)
#   - is None narrowing on properties (Bug A fix)
#   - property cast through unions (resolve_union_receiver)
#   - SSU phi cascade at post-if merges (Bug B fix)
#   - real identity via `is` on non-None operands (prim_is)
#   - while True with break (issue 005 GLOBAL_CONTOUR guard)
#   - inliner bail on both-SUM substitution (inline.cc)
#   - parent/child cross-pointers, multi-tree linking
#
# No workarounds remain.  Mixed-element list literals work
# after the FA fix in clone.cc:concretize_var_list_type
# (was: when a list's element-type was a union of multiple
# distinct cs->types, the old code overwrote the freshly-
# cloned `sym_list` Var.type with the element-type SUM —
# leaving the Var typed as `Node | nil_type` instead of as
# a list, which then drove method dispatch through
# `_CG_any` formals).

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None
    self.prev = None
    self.parent = None
    self.child = None
    self.degree = 0

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

  def consolidate(self):
    if self.min is None:
      return
    dummy = Node(0)
    # Roots list — mixed-element literal seeds the element
    # type as `Node | None`, then nullified before appending
    # the real roots.  Works after the voidish-arg-cast fix
    # in cg.cc:write_send_arg (this commit).
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
    # Degree table — same pattern.
    A = [dummy, None, dummy, None, dummy, None, dummy, None]
    i = 0
    while i < 8:
      A[i] = None
      i = i + 1
    # Process each root.
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
    # Rebuild root list from A.
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

h = Heap()
h.insert(5)
h.insert(3)
h.insert(7)
h.insert(1)
h.insert(9)
h.insert(2)
h.insert(8)
h.insert(4)
print(h.n)

i = 0
while i < 8:
  print(h.extract_min())
  i = i + 1
print(h.n)
print(h.extract_min())
