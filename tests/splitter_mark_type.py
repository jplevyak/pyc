# ifa/issues/074: the MARK_TYPE splitter (stage 2), which is OFF by
# default since 2026-08-15 -- `.env` turns it back on so this test can
# demonstrate what demands it.
#
# Two list comprehensions over lists of different element types. The two
# `list.append` call edges carry distinct types, but pyc names contours by
# tuples of type SETS, so once {A,B} forms at append's value formal it is a
# fixed point -- every edge carries {A,B}, so etype == stype and
# TYPE_CONFLUENCE has nothing left to see. MARK_TYPE is the only stage that
# can break that symmetry, because marks separate contributors by
# provenance even when their type sets are identical.
#
# This is the precision that marks-off costs; see
# tests/listcomp_element_separation.py for the same shape at the default
# settings, where the union survives.
class A:
    def ay(self):
        return 1

class B:
    def bee(self):
        return 2

class Holder:
    def __init__(self):
        self.aas = []
        self.bbs = []

    def fill(self):
        self.aas.append(A())
        self.bbs.append(B())

    def prune(self):
        self.bbs = [x for x in self.bbs]
        self.aas = [y for y in self.aas]

    def use(self):
        return self.aas[-1].ay()

h = Holder()
h.fill()
h.prune()
print(h.use())
