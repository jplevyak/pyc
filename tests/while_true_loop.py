# issues/005: `while True:` used to segfault FA in update_in --
# `True` is a constant whose AVar lives in the global contour, and
# analyze_edge sets is_if_arg on it; update_in then dereferenced the
# old (void*)1 sentinel. Now structurally safe: GLOBAL_CONTOUR is a
# real EntrySet (ifa/issues/031 step 1). This is the committed
# regression carrier for the shape (the fibheap tests that also
# exercise it arrived with issue 028).
count = 0
while True:
  count = count + 1
  if count > 3:
    break
print(count)

# The motivating fib-heap idiom: circular-list walk with `while
# True:` + identity comparison, at module level (reads go through
# issue-031 step 2 load temps).
class Node:
  def __init__(self, k):
    self.key = k
    self.next = None

head = Node(1)
b = Node(2)
c = Node(3)
head.next = b
b.next = c
c.next = head

total = 0
w = head
while True:
  total = total + w.key
  w = w.next
  if w is head:
    break
print(total)

# Nested: `while True:` inside a function, breaking on a global
# mutated by a callee. NOTE: the loop is deliberately not the
# function's first statement -- `while True:` as the very first
# PNode of a function is a separate, pre-existing bug (FA "'n' has
# no type" + wrong dispatch on both backends; see
# issues/025-while-true-first-statement-of-function.md).
done = False
steps = 0

def step():
  global done, steps
  steps = steps + 1
  if steps >= 5:
    done = True

def run():
  local_guard = 0
  while True:
    step()
    if done:
      break
  return local_guard

run()
print(steps)
