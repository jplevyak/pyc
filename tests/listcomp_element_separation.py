# Two structurally identical list comprehensions over lists of different
# element types. Both accumulators start as one merged `list` CreationSet,
# so both `append`s share one EntrySet whose value formal is {A, B}; that
# contour must be split (PYC_ESBLOCK) before the CreationSet can be. The
# check: `aas`' element stays A, so `a.ay()` types with no violation.
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
