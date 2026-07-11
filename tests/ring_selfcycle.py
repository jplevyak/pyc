# Single-node circular ring (next and prev point to self).
# Smallest case of a pointer self-cycle in a CS.
#
# Walking x.next.next.next... must terminate at the same x
# in IFA's per-CS field tracking; otherwise the CS would
# proliferate or n.next.key would acquire spurious types.

class Node:
  def __init__(self, k):
    self.key = k
    self.next = None
    self.prev = None

x = Node(7)
x.next = x
x.prev = x

print(x.key)              # 7
print(x.next.key)         # 7
print(x.next.next.key)    # 7
print(x.prev.key)         # 7
print(x.next.prev.key)    # 7
