# ifa/issues/110: a subclass that overrides a method must not get a
# SECOND member slot for it.
#
# `has` indices are the emitted struct's field suffixes, and a method
# shared between a base and a subclass is emitted once, taking a generic
# receiver and blind-casting to the base's layout. That is sound only
# while the subclass's layout is a prefix-compatible extension of the
# base's. An override appended at the end broke it: every field after
# the duplicated slot shifted by one, so an inherited sibling method
# read the override's function pointer where the field should be.
#
# Each loop below is correct in isolation -- it takes two live receiver
# types through one shared method to defeat monomorphization, which is
# why nothing in the suite caught this before.


class B:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def more(self):
        return self.i < self.n

    def step(self):
        self.i = self.i + 1


class S(B):
    def step(self):
        self.i = self.i + 2


a = B(3)
while a.more():
    a.step()
print(a.i)

b = S(3)
while b.more():
    b.step()
print(b.i)


# Three levels, an override at two of them, and a field only the leaf
# adds -- exercises the recursion's dedup (a grandparent's members used
# to arrive twice) as well as the in-place override merge.
class A2:
    def __init__(self, n):
        self.i = 0
        self.n = n

    def more(self):
        return self.i < self.n

    def step(self):
        self.i = self.i + 1

    def tag(self):
        return "A2"


class B2(A2):
    def step(self):
        self.i = self.i + 2


class C2(B2):
    def __init__(self, n):
        A2.__init__(self, n)
        self.extra = 7

    def tag(self):
        return "C2"


x = A2(4)
while x.more():
    x.step()
print(x.tag(), x.i)

y = B2(4)
while y.more():
    y.step()
print(y.tag(), y.i)

z = C2(4)
while z.more():
    z.step()
print(z.tag(), z.i)
print(C2(4).extra)
