# ifa/issues/074: the CARTESIAN_PRODUCT splitter (stage 1b, PYC_CPA),
# which is OFF by default -- `.env` enables it at a union cap of 4.
#
# Same program as tests/splitter_mark_type.py: a union that is a fixed
# point under set-naming. CPA attacks it from the other side, fanning the
# contour into one per single CreationSet so the union never becomes a
# contour NAME, where MARK_TYPE instead separates the contributors by
# provenance.
#
# The recorded STAGES line also shows CPA's perturbation waking three
# stages that never run at the default settings on any program measured so
# far -- VIOLATION and PER_CS_RECEIVER included. That is the cascade's
# first-stage-wins starvation made visible, and it is the main thing this
# test pins.
#
# COMPILE-ONLY on purpose (no .exec.check): PYC_CPA's callee-side-only fan
# leaves contours that no edge reaches, so the binary aborts. That is the
# flag's documented negative result (074), not something this test should
# pin -- what it asserts is the stage set.
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
