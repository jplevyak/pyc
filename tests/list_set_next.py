# Linked list with Node creation inside a function that
# assigns to another Node's field.
#
# Before issue 026's mark_live fix, this returned 10 (5+5)
# instead of 8 (5+3) because:
#   1. pyc constant-folded `value=5` on the outer Node CS
#      and the inside-function Node CS independently.
#   2. The dead-code analysis short-circuited backward
#      propagation through ivs whose AVar had a single
#      constant — so the field's Var->live stayed 0 even
#      though sum_list reads the field.
#   3. The struct elided the dead field; the period read
#      codegen still emitted `obj->e1` referencing the
#      missing slot.
#
# The fix: in mark_live_avar, don't short-circuit on
# get_constant for AVars whose contour is a CreationSet
# (i.e. for instance variables / struct field AVars).
# These need their backing struct slot kept alive when
# any period read of the field is emitted, even if the
# per-CS value is a single constant.

class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def set_next(node, v):
  node.next = Node(v)

def sum_list(n):
  if n is None:
    return 0
  return n.value + sum_list(n.next)

t = Node(5)
set_next(t, 3)
print(sum_list(t))  # 5 + 3 = 8
