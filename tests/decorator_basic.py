# issues/007: user-defined decorators are applied. The closure-
# wrapping shape (the standard decorator idiom), a decorator
# returning a different existing function, stacking (bottom-up
# application), and a parameterized decorator (@d(args) applied as
# d(args)(fn)).
def double(f):
    def wrapper(x):
        return f(x) * 2
    return wrapper

@double
def add_one(x):
    return x + 1

print(add_one(5))

def replacement(x):
    return x + 100

def swap(f):
    return replacement

@swap
def add_two(x):
    return x + 2

print(add_two(5))

def add_n(n):
    def dec(f):
        def wrapper(x):
            return f(x) + n
        return wrapper
    return dec

@add_n(30)
def base(x):
    return x

print(base(7))

# NOTE: STACKED decorators built from the same closure-wrapping
# decorator (@double @double) remain unsupported: the inner wrapper
# value is a closure-carrier instance, and dispatching a mixed
# raw-fn/carrier-instance candidate set through one call site needs
# carrier classes to carry a method-pointer slot (and unique class
# names) -- tracked as remaining scope in ifa/issues/030.
