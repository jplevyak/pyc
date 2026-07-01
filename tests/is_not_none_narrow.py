class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def f(n):
  if n is not None:
    return n.value
  return 0

print(f(Node(5)))
print(f(None))
