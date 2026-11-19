# ifa/210: a function used as a VALUE, in every place one can be read: a list
# and a tuple display, a field, a return, an argument, a dict value and an
# append. One rule on both backends: the function's address.
def inc(x):
    return x + 1


class Holder:
    def __init__(self, f):
        self.f = f


def pick(n):
    if n:
        return lambda x: x * 3
    return lambda x: x * 4


def apply(f, v):
    return f(v)


fs = [lambda x: x - 7, lambda x: x * x]          # list display
for k in fs:
    print(k(5))
t = (inc, lambda x: x * 10)                       # tuple display
print(t[0](1), t[1](2))
h = Holder(lambda x: x + 100)                     # field store
print(h.f(1))
h.f = inc
print(h.f(1))
print(pick(1)(2), pick(0)(2))                     # returned
print(apply(lambda x: x - 1, 9), apply(inc, 9))   # argument
d = {"a": lambda x: -x, "b": inc}                 # dict values
print(d["a"](3), d["b"](3))
g = [inc]
g.append(lambda x: x * 2)                         # appended
print([q(4) for q in g])
