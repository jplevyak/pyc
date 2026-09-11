# ifa/133: the same two comprehensions as listcomp_element_separation.py,
# run start-merged (PYC_CSDCPA1=2 -- one CreationSet per sym, split on
# demand). This is the case CLAUDE.md's dependency exists for:
#
#   "an EntrySet is split SO THAT a CreationSet split becomes possible,
#    when a demand test has asked for one."
#
# Start-merged, both comprehension results share one `list` CreationSet, so
# BOTH appends have receiver type {list#1060} and share ONE `append`
# contour whose value formal is the union {A, B}:
#
#   es=60 args= [append] [list#1060] [A B]
#     <- edge=87 from=prune es=50 args= [append] [list#1060] [A B]
#     <- edge=86 from=prune es=50 args= [append] [list#1060] [A B]
#
# Route 4 then declines "1 group: every creation point on the same assign
# sets" -- not because the creation points are alike, but because the walk
# crosses that shared contour and reaches both from either end.
#
# Note the two in-edges come from the SAME caller contour with IDENTICAL
# actual types, so there is nothing type-shaped to split `append` on. That
# is why every attempt keyed on the call site became a fan (ifa/144's
# signature: partition size = caller count). PYC_ESBLOCK instead finds the
# blocking contour BY TEST -- hold its formals terminal, do the creation
# points separate? -- and groups its in-edges by which creation points they
# reach: the demand's own objects, bounded by their number.
#
#   [esblock]    p=2 cs=1060 BLOCKER es=60 fun=append
#   [esblock]    p=2 cs=1060 SPLIT es=60 edges=2 -> 2 group(s)
#   [csdefsplit] p=3 cs=1060 def av=3130 -> cs=1062 (group 1/2 sig=110)
#
# The ES split is the MEANS; route 4 does the CreationSet split itself on
# the next pass with its own content key, and `append` ends with x={B} on
# one contour and x={A} on the other. Without PYC_ESBLOCK this program
# emits "illegal call argument type 'a' illegal: B" on this arm.
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
