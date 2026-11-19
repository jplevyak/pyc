# issues/182: a keyword-only parameter takes no positional argument.
# CPython: TypeError: g() takes from 1 to 2 positional arguments but 3 were given
def g(a, b=2, *, c=3):
    return a + b + c

print(g(1, 2, 3))
