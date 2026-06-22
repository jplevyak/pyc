# Doubly-linked list with both prev and next pointers.
# Recursive sum_forward walks the .next chain.
#
# Exercises issue 026's struct-field-numbering fix.  Before
# the fix, the prev field was elided from the struct
# (because it's written but never read by sum_forward),
# leaving a hole in the eN index numbering — and the
# setter codegen for `n2.prev = head` referenced that
# hole, producing a C compile error
# ("struct has no member named e2").
#
# After the fix, the struct includes every typed field
# even when its Var is otherwise dead.  The prev writes
# now have a valid struct slot.

class Node:
  def __init__(self, v):
    self.value = v
    self.prev = None
    self.next = None

def sum_forward(node):
  if node is None:
    return 0
  return node.value + sum_forward(node.next)

# Build:  head(1) <-> n2(2) <-> n3(3)
head = Node(1)
n2 = Node(2)
n3 = Node(3)
head.next = n2
n2.prev = head
n2.next = n3
n3.prev = n2

print(sum_forward(head))   # 1+2+3 = 6
print(sum_forward(n2))     # 2+3 = 5
print(sum_forward(n3))     # 3
