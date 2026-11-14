# issues/189: more elements than the callee has positional parameters is
# CPython's TypeError, refused with its message.
def f(a, b):
    return a - b

print(f(*(1, 2, 3)))
