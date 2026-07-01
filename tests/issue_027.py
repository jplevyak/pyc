class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def sum_all(head):
  total = 0
  node = head
  while node is not None:
    total = total + node.value
    node = node.next
  return total

n1 = Node(10)
n2 = Node(20)
n1.next = n2
print(sum_all(n1))
