# sum() of floats is Neumaier compensated summation in CPython 3.12+
# (Python/bltinmodule.c, builtin_sum_impl), not a left-to-right loop: the
# two differ in the last bit, and a naive sum loses small terms entirely.
# ant's path lengths differed from CPython's by one ULP, which then changed
# which path won a comparison.
import random

print(sum([0.1] * 10, 0.0), sum([0.1, 0.2, 0.3], 0.0))
print(sum([1e16, 1.0, -1e16], 0.0), sum([0.1] * 10 + [1e16, 1.0, -1e16], 0.0))
print(sum([1.5, 2.5], 0.25), sum([-0.0], -0.0))
print(sum([float("inf"), 1.0], 0.0), sum([1e308, 1e308], 0.0))
random.seed(3)
xs = [random.random() * 100 for i in range(1000)]
print(repr(sum(xs, 0.0)))
ys = [random.random() - 0.5 for i in range(1000)]
print(repr(sum(ys, 0.0)), repr(sum(ys, 1e10)))
# ints and lists are untouched
print(sum([1, 2, 3]), sum([]), sum([], 5), sum(range(10)))
print(sum([[1], [2, 3]], []))
