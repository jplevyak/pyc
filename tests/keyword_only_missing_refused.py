# issues/182: a keyword-only parameter with no default must be passed.
# CPython: TypeError: h() missing 1 required keyword-only argument: 'k'
def h(a, *args, k):
    return a * k + len(args)

print(h(2, 3))
