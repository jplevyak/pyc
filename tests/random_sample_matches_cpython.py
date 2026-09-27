# random.sample follows CPython's algorithm, so a seeded run draws the
# same sequence (both the pool and the selection-set branch).
import random
random.seed(3)
big = list(range(500))
print(random.sample(big, 20))
print(random.sample(list(range(30)), 7), random.sample(list(range(10)), 3))
print(random.sample(big, 5), random.choice(big))
