# A keyword argument that provides a LATER positional while an
# EARLIER default goes unfilled (regression: default_wrapper assumed
# defaults fill a contiguous tail, so the kwarg's value landed in the
# defaulted slot and the wrong default landed in the kwarg's slot --
# circle.py's `pack([c], exclude=c)` bound damping to a Circle and
# exclude to None, silently).
def f(a, b=10, c=20):
    return a + b * 2 + c * 3

print(f(1))
print(f(1, c=5))
print(f(1, b=4))
print(f(1, 2, 3))

class P:
    def __init__(self, x):
        self.x = x

def pack(items, damping=0.5, exclude=None):
    total = 0.0
    for it in items:
        if it is not exclude:
            total += it.x * damping
    return total

p = P(2.0)
q = P(4.0)
print(pack([p, q], exclude=p))
print(pack([p, q]))
