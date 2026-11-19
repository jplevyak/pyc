# issues/182: the parameters after a bare `*` are keyword-only: bound by
# name, with their defaults, in functions, methods and constructors.
def g(a, b=2, *, c=3):
    return a + b + c
print(g(1, c=5))
print(g(1))
print(g(1, 10))
print(g(c=1, a=2))

def h(a, *, k):
    return a * k
print(h(2, k=4))

class P:
    def __init__(self, x, *, scale=1):
        self.x = x * scale
    def get(self, *, plus=0):
        return self.x + plus
p = P(3, scale=2)
print(p.x, p.get(), p.get(plus=5))
print(P(4).x)
