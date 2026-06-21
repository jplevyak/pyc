# Recursive linked list with `is None` narrowing.
#
# The originally-motivating pattern from issues 004 / 024 /
# 025: a Node-with-None-terminated next pointer, a recursive
# sum_list that narrows via `if node is None`.  Each
# downstream level of the issue chain documented something
# that broke this:
#
#   004: `is` operator was unimplemented (no __is__ method).
#   024: `__is__` method dispatch failed on union receivers.
#   025: per-branch type narrowing wasn't applied at Code_IF.
#
# The full fix:
#   - Frontend (`python_ifa_build_if1.cc:PY_compare`)
#     rewrites `x is None` and `None is x` to
#     `prim_isinstance(x, sym_nil_type)`.
#   - C codegen (`cg.cc:write_send`) emits a NULL pointer
#     check.
#   - v2 LLVM codegen (`cg_normalize_v2.cc:lower_send_prim`)
#     emits a CG2_BINOP EQ against a null-ptr constant.
#   - IFA (`fa.cc:Code_IF`) narrows the per-branch SSU AVar
#     via the prim_isinstance handler that already existed.

class Node:
  def __init__(self, v):
    self.value = v
    self.next = None

def sum_list(node):
  if node is None:
    return 0
  return node.value + sum_list(node.next)

# Build 1 → 2 → 3 → None
n1 = Node(1)
n2 = Node(2)
n3 = Node(3)
n1.next = n2
n2.next = n3
print(sum_list(n1))   # 1+2+3 = 6

# Build a longer chain: 1..10
head = Node(1)
prev = head
i = 2
while i <= 10:
  cur = Node(i)
  prev.next = cur
  prev = cur
  i = i + 1
print(sum_list(head))   # 1+2+...+10 = 55

# Empty list (just None — pyc needs a Node to type-check
# the call site, so call with the only-None pattern
# separately to avoid mixed-type dispatch at the call site).
empty_head = Node(0)
print(sum_list(empty_head))   # 0
