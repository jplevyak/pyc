class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def set_next(node, v):
  node.next = Node(v)

def sum_list(n):
  if n is None: return 0
  return n.value + sum_list(n.next)

t = Node(5)
set_next(t, 3)
print(sum_list(t))
