# ifa/200 A3: a call site whose candidates include an unnamed function (a
# capturing lambda, dispatched through its closure carrier) next to a plain
# function or a nested def. The LLVM backend bailed the whole site on the
# lambda's missing name, and its fallback dispatch then fell through with
# no branch taken, so every call printed 0. The C backend never required a
# name. The condition is a run-time value, so FA keeps both candidates.
import sys


def make(n):
    return lambda x: x + n


def make2(n):
    def add(x):
        return x * n
    return add


def inc(x):
    return x + 1


few = len(sys.argv) < 5
g = make(3) if few else inc
print(g(10))
h = inc if few else make(4)
print(h(10))
k = make(3) if few else make2(2)
print(k(10))
m = make2(5) if few else make(4)
print(m(10))
