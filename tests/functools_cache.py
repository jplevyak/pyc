# functools.lru_cache, cache, wraps and cmp_to_key (issues/207).
import functools
from functools import lru_cache, cache, wraps, cmp_to_key, reduce

@lru_cache(maxsize=None)
def fib(n):
    return n if n < 2 else fib(n - 1) + fib(n - 2)
print(fib(80), fib.cache_info())

@cache
def fact(n):
    return 1 if n < 2 else n * fact(n - 1)
print(fact(20), fact.cache_info())
fact.cache_clear()
print(fact.cache_info())

@lru_cache(maxsize=2)
def dbl(x):
    print("compute", x)
    return x * 2
print(dbl(1), dbl(2), dbl(1), dbl(3), dbl(2), dbl(1))
print(dbl.cache_info())

@lru_cache
def sq(x):
    return x * x
print(sq(4), sq(4), sq.cache_info())

@lru_cache()
def join2(a, b):
    return a + "-" + b
print(join2("x", "y"), join2("x", "y"), join2.cache_info())

def deco(f):
    @wraps(f)
    def inner(x):
        return f(x) + 1
    return inner

@deco
def plus(x):
    return x * 10
print(plus(4))

def cmp(a, b):
    return (a > b) - (a < b)
print(sorted([3, 1, 2], key=cmp_to_key(cmp)), sorted(["bb", "a", "ccc"], key=cmp_to_key(lambda a, b: len(b) - len(a))))
print(reduce(lambda a, b: a * b, [1, 2, 3, 4]), reduce(lambda a, b: a + b, [1, 2], 10))
