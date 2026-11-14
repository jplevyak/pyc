# issues/189: a list's `*` has no length known at compile time, so it cannot
# be bound to named parameters (into `*args` it can: call_unpacking.py).
def f(a, b):
    return a - b

print(f(*[1, 2]))
