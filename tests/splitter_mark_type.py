# ifa/issues/074 / ifa/issues/146 D: this used to demonstrate the MARK_TYPE
# splitter, which was REMOVED 2026-09-08. Mark distance is
# depth-from-a-generating-AVar -- provenance -- so no type tuple can name
# what it separates, and CLAUDE.md's rule retires it.
#
# The fixture is kept because the SHAPE it builds is the valuable part, and
# ifa/142 cites the paragraph below for it. What it now pins is that the
# shape costs NOTHING to resolve without marks: `CALLS: direct=69
# dynamic=0`, byte-identical to the numbers MARK_TYPE produced. The STAGES
# line lost `MARK_TYPE` and gained nothing, which is the point -- the
# remaining stages reach the same answer.
#
# Two list comprehensions over lists of different element types. The two
# `list.append` call edges carry distinct types, but pyc names contours by
# tuples of type SETS, so once {A,B} forms at append's value formal it is a
# fixed point -- every edge carries {A,B}, so etype == stype and
# TYPE_CONFLUENCE has nothing left to see. MARK_TYPE is the only stage that
# can break that symmetry only by looking past the types. MARK_TYPE did
# that with provenance, which is why it went; CS_DEF_PARTITION now fires on
# this fixture and reaches the same call resolution without it.
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
