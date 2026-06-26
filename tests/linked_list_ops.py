# Linked list with multiple recursive operations over the
# same Node | None type.  Tests `is None` narrowing in a
# variety of patterns: find, length, max.  Construction is
# encapsulated in `build_list`.
#
# Pre-issue-026 (Bug 5 fix), recursive-self-mutation inside
# a function would have struct-collapsed the Node fields.
# That's now fixed, so the construction can live in a
# function with no special pattern.

class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def length(node):
  if node is None:
    return 0
  return 1 + length(node.next)

def find_max(node, best):
  if node is None:
    return best
  if node.value > best:
    return find_max(node.next, node.value)
  return find_max(node.next, best)

def find(node, target):
  if node is None:
    return 0
  if node.value == target:
    return 1
  return find(node.next, target)

def build_list(vals):
  head = Node(vals[0])
  prev = head
  i = 1
  while i < len(vals):
    cur = Node(vals[i])
    prev.next = cur
    prev = cur
    i = i + 1
  return head

# Build 3 -> 1 -> 4 -> 1 -> 5 -> 9 -> 2 -> 6 -> None
vals = [3, 1, 4, 1, 5, 9, 2, 6]
head = build_list(vals)

print(length(head))                  # 8
print(find_max(head, head.value))    # 9
print(find(head, 4))                 # 1 (found)
print(find(head, 100))               # 0 (not found)
