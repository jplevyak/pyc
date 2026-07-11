# Circular doubly-linked ring with a 4-pointer splice.
# Builds a 2-node ring (a, b), then splices c in between
# them via the classic insert-after sequence:
#
#   c.next = a.next      # c points forward to b
#   c.prev = a           # c points back to a
#   a.next.prev = c      # b's back-pointer now c
#   a.next = c           # a's forward-pointer now c
#
# After: a -> c -> b -> a (forward), a -> b -> c -> a (back).
# Sum walking the forward ring once visits each key.

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None
    self.prev = None

# 2-node ring
a = Node(1)
b = Node(2)
a.next = b
a.prev = b
b.next = a
b.prev = a

# Splice c between a and b
c = Node(3)
c.next = a.next      # c -> b
c.prev = a           # c <- a
a.next.prev = c      # b.prev = c
a.next = c           # a.next = c

# Forward walk from a: a(1) -> c(3) -> b(2) -> a (stop)
print(a.key)                  # 1
print(a.next.key)             # 3
print(a.next.next.key)        # 2
print(a.next.next.next.key)   # 1 (back to a)

# Backward walk from a: a(1) -> b(2) -> c(3) -> a (stop)
print(a.prev.key)             # 2
print(a.prev.prev.key)        # 3
print(a.prev.prev.prev.key)   # 1

# Sum across the ring (3 hops)
total = a.key + a.next.key + a.next.next.key
print(total)                  # 6
