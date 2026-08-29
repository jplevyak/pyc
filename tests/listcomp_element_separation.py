# ifa/issues/074: two list comprehensions over different element types.
# The two `append` call edges carry distinct types, but pyc names contours
# by tuples of type SETS, so once {A,B} forms at append's value formal it
# is a fixed point -- every edge carries {A,B}, so etype == stype and
# TYPE_CONFLUENCE has nothing to split. Only MARK_TYPE could break the
# symmetry (via provenance), which is why disabling it costs precision
# here. The equivalent explicit for/append loops separate fine.
#
# Runs correctly either way; this guards the answer, not the analysis.
#
# The warning this used to record is GONE as of ifa/issues/050 stage 1:
# `h` is a module-level cell, and resolving its load to the store that
# dominates it (IFACallbacks::provably_constant_load) keeps `aas`' element
# type at A, so the "illegal call argument type 'a' illegal: B" line no
# longer appears. The contour-naming limitation above is unchanged --
# what changed is that this program no longer reaches it.
class A:
    def __init__(self):
        self.dead = False
    def ay(self):
        return 1

class B:
    def __init__(self):
        self.dead = False
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
        # two structurally identical comprehensions, different element types
        self.bbs = [x for x in self.bbs if not x.dead]
        self.aas = [y for y in self.aas if not y.dead]

    def use(self):
        a = self.aas[-1]
        return a.ay()          # legal only if aas' element stayed A

h = Holder()
h.fill()
h.prune()
print(h.use())
