# random: choices, gauss and the variates, randrange with a step, randbytes, drawing CPython's values for a seed (issues/207).
import random
random.seed(7)
print(random.choices("abcde", k=5), random.choices([1, 2, 3], weights=[10, 1, 1], k=6), random.choices("xy", cum_weights=[1, 3], k=4))
print(random.gauss(), random.gauss(), random.gauss(5.0, 2.0), random.normalvariate(1.0, 0.5), random.expovariate(2.0))
print(random.randrange(10), random.randrange(5, 50, 5), random.randrange(10, 0, -3), random.randrange(3, 9))
random.seed(7)
print(random.gauss(), random.lognormvariate(0.0, 1.0), random.randbytes(4), random.randbytes(7), random.randbytes(1))
try:
    print(random.randrange(0))
except ValueError as e:
    print("ValueError", e)
try:
    print(random.randrange(5, 5))
except ValueError as e:
    print("ValueError", e)
try:
    print(random.randrange(1, 10, 0))
except ValueError as e:
    print("ValueError", e)
try:
    print(random.randrange(10, 1, 2))
except ValueError as e:
    print("ValueError", e)
try:
    random.choices("ab", weights=[0, 0])
except ValueError as e:
    print("ValueError", e)
