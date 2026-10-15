# ifa/132: one slot holding two lists of ONE class with different arity.
#
# Each fixed-length literal gets a record layout, and `Box.v` must hold both
# the 1-element and the 3-element one, so codegen gives the slot no C type
# (`_CG_void`, read as `_CG_any`; IFA_DBG_SLOTREP reports the conflict).
# That is sound: a record list is allocated as a real list (header plus
# inline elements, `_CG_TUPLE_TO_LIST_FUN`), and every access is resolved
# from the analysis types. This pins that down -- reads, iteration,
# mutation, slicing, comparison and concatenation through the shared slot,
# and a function returning either arity. PYC_SLOTARITY, which demoted such
# literals to list layout, was deleted because this already works.


class Box:
    def __init__(self, n):
        if n:
            self.v = [n, n + 1, n + 2]
        else:
            self.v = [7]

    def total(self):
        t = 0
        for x in self.v:
            t += x
        return t


def pick(c):
    if c:
        return [1, 2]
    return [4, 5, 6]


bs = [Box(0), Box(3)]
for b in bs:
    print(len(b.v), b.v[0], b.v[-1], b.total())
for b in bs:
    b.v.append(9)
    b.v[0] = 100
    w = b.v[1:]
    print(len(b.v), b.v[0], b.v[-1], len(w), sum(w), b.v == [100, 9], 9 in b.v)
x = bs[0].v + bs[1].v
print(len(x), x)

a = pick(len(bs) > 5)
c = pick(len(bs) < 5)
print(len(a), a[0], a[-1], sum(a))
print(len(c), c[0], c[-1], sum(c))
