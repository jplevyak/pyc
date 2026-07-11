# Fib-heap insert + minimum WITHOUT the `m = self.min`
# local-binding workaround.  Tests issue 028 Bug A: a
# fresh `self.min.next` read in the else branch of
# `if self.min is None:` must codegen against the Node
# component of the (None | Node) union, not against
# `_CG_nil_type`.
#
# Before the fix in cg.cc's resolve_union_receiver,
# this file failed C compile with:
#   error: 'void*' is not a pointer-to-object type
#       t34 = (_CG_ps3610)((_CG_nil_type)t36)->e1; /* next */
#
# After: codegen picks the non-nil component of the
# union receiver, emitting `(_CG_ps_Node)t36->e1`.

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
      # Fresh property reads in the else branch — these used
      # to fail codegen.
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

h = Heap()
print(h.minimum())     # -1
h.insert(5)
print(h.minimum())     # 5
h.insert(3)
print(h.minimum())     # 3
h.insert(7)
print(h.minimum())     # 3
h.insert(1)
print(h.minimum())     # 1
print(h.n)             # 4
