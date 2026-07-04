# ifa/issues/031 step 2: module-level variables are memory cells
# whose every read is a fresh load (a per-read local temp in IF1),
# never a cached register. Exercises: rebinding a global closure
# and observing the new binding on the next call; rebinding a
# global object and re-reading fields; a loop whose condition
# re-reads a global mutated by a callee each iteration; and
# store/read interleaving at module top level.
class Counter:
  v = 0
  get = lambda y: y.v

stash = None

def setup(n):
  global stash
  c = Counter()
  c.v = n
  stash = c.get

def use():
  print(stash())

setup(10)
use()
setup(32)
use()

# Global object rebinding: field reads must follow the cell.
class Box:
  def __init__(self, k):
    self.key = k

b = Box(1)

def rebox(k):
  global b
  b = Box(k)

print(b.key)
rebox(5)
print(b.key)

# Loop condition re-reads a global mutated by the callee.
count = 0
done = False

def step():
  global count, done
  count = count + 1
  if count >= 3:
    done = True

while not done:
  step()
print(count)

# Store/read interleaving at module top level.
total = 0
total = total + b.key
total = total + count
print(total)
