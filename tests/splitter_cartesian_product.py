# ifa/issues/074 / ifa/issues/146 E: this used to demonstrate the
# CARTESIAN_PRODUCT splitter (PYC_CPA), which was REMOVED 2026-09-08 as
# arbitrary splitting.
#
# CPA fanned a contour into one filtered contour per single CreationSet
# whenever a positional formal's live type held 2..N CreationSets. That
# trigger asks for no demand: the function contained zero references to
# violation, irrepresentable, dispatch or unresolved, so it fired whether or
# not anything downstream was harmed by the union. A union's EXISTENCE is a
# fact about the program; a demand is something OBSERVING a distinction and
# being unable to proceed.
#
# The fixture is KEPT because the shape is the valuable part: a list
# comprehension that rebuilds `self.aas` from itself, so the element type is
# a union that no type test can name (every edge carries it -- ifa/142's
# fixed point). CPA attacked it by refusing to let the union become a
# contour NAME.
#
# What this now pins is the ACCEPTANCE TEST for CPA's replacement. Removing
# CPA improves call resolution here (direct=69 dynamic=0, against CPA's
# 68/1) but costs the element separation, so `.ay()` on a B draws a warning.
# The .check records the state WITHOUT that warning, and .known_issue
# explains the gap; the test flips to PASS when demand-driven receiver
# filtering lands and reaches the same separation without the blind fan.

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
