# issues/189: the forwarding decorator. `fn(*a, **kw)` passes `kw` to a
# function with named parameters, which needs the dict's keys in its type.
def log(fn):
    def inner(*a, **kw):
        return fn(*a, **kw)
    return inner

@log
def g(x, y):
    return x - y

print(g(5, 2))
