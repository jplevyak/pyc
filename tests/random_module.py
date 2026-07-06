# Regression: the pyc `random` stdlib shim (pyc_lib/random.py, an LCG).
# Its stream does not match CPython, so assert only invariants that
# hold for any correct PRNG (ranges, membership, multiset preserved by
# shuffle). Deterministic given the seed. (issue 025 bucket C.)
import random
from random import randint, choice

def main():
    random.seed(42)
    r = random.random()
    print(0.0 <= r < 1.0)
    print(1 <= randint(1, 6) <= 6)
    xs = [10, 20, 30, 40]
    print(choice(xs) in xs)
    random.shuffle(xs)
    print(len(xs) == 4)
    print(10 in xs and 20 in xs and 30 in xs and 40 in xs)
    print(0 <= random.randrange(0, 100) < 100)
    u = random.uniform(5.0, 6.0)
    print(5.0 <= u <= 6.0)

main()
