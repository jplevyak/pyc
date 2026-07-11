# Recursive insert that creates a Node and returns it (issue
# 026 Bug 5).  The failure mode before the fix:
#
#   sum_list returned 5+5=10 instead of 5+3=8 because pyc's
#   `is None` narrowing in sum_list snapshotted operand CSs
#   at constraint-setup time.  Later iterations brought
#   Node(3) into the operand's union, but the snapshot
#   restrict={Node(5)} filtered it out — so n.value got
#   constant-folded to 5 in the non-None branch.
#
# Fix: narrowing for `is None` now installs a type-level
# predicate restrict on the SSU view, re-evaluated as new
# CSs arrive.  See ifa/issues/026 and
# ifa/analysis/NOTES.md.

class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def insert(node, v):
  if node is None:
    return Node(v)
  node.next = insert(node.next, v)
  return node

def sum_list(n):
  if n is None:
    return 0
  return n.value + sum_list(n.next)

t = Node(5)
t = insert(t, 3)
print(sum_list(t))    # 5 + 3 = 8

t = insert(t, 7)
print(sum_list(t))    # 5 + 3 + 7 = 15

t = insert(t, 1)
print(sum_list(t))    # 5 + 3 + 7 + 1 = 16
