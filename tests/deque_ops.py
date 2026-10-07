# collections.deque: popleft is O(1) amortized (it was list.pop(0), O(n),
# which made sokoban's breadth-first search 34x slower than CPython), plus
# appendleft, extend, indexing, and IndexError on an empty pop.
from collections import deque
q = deque()
for i in range(10):
    q.append(i)
q.appendleft(-1)
print(q.popleft(), q.popleft(), len(q), q[0], q[-1], list(q))
total = 0
for i in range(300000):
    q.append(i)
while q:
    total += q.popleft()
print(total, len(q))
q.extend([7, 8])
print(q.pop(), list(q))
try:
    deque().popleft()
except IndexError:
    print("empty")
