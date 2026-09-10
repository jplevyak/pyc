# ifa/issues/143 (and 133): two ARITY-1 list literals holding different
# types must not share a CreationSet.
#
# This reproduces `bh`'s SIGNATURE, not (as first claimed) its instance --
# see the note at the bottom. That program's failure has the same shape:
#
#     class Random: __slots__ = ["seed"]      # arity-1 str literal
#     self.bodies  = [None] * nbody          # arity-1 literal, multiplied
#
# CreationSet identity is (sym x arity) -- arity participates because it is
# REPRESENTATION (ifa/132: a record-able container's layout is a struct of
# exactly vars.n fields), and under PYC_CSDCPA1 everything else about a
# list's identity is deliberately merged. So `["seed"]` and `[None]` are
# both `list` of arity 1 and land on ONE CreationSet, whose slot 0 unions
# {str, None}.
#
# `list.__mul__` is P_prim_merge, and structural_assignment(..., merge=true)
# pours the OPERAND's positional slots into the RESULT's element -- so the
# str reaches `self.bodies`'s element channel. `__setitem__` then adds Body,
# giving {str, Body}, and `reversed(self.bodies)` hands that union to the
# loop variable.
#
# WHY THE MERGE IS NOT CAUGHT AT THE TIME: at the literal, slot 0 is
# {str, None}, and that IS representable -- elem_irrepresentable skips nil
# and finds a single basic -- so no demand is raised where the merge
# happens. The union only becomes irrepresentable several contours
# downstream, by which point every creation point carries it and the
# attribution is gone (ifa/133, "the demand is unobservable at the moment of
# the merge").
#
# NOT covered by tests/list_mul_scalar_object_separation.py, which is the
# richards shape ([0]*4 against [None]*4) and which the ifa/133 nilstore fix
# resolved on both arms 2026-09-09. This one has a str on the far side and
# an UNMULTIPLIED literal, and that fix does not reach it.
#
# STATUS 2026-09-10: passes at the DEFAULT; under PYC_CSDCPA1=2 emits
# `illegal call argument type 'b' illegal: str` -- bh's exact signature.
class Body:
    def __init__(self, n):
        self.mass = n

    def tag(self):
        return self.mass


class Random:
    __slots__ = ["seed"]          # arity-1 str literal, NOT multiplied

    def __init__(self):
        self.seed = 1


class Tree:
    def __init__(self, n):
        self.bodies = [None] * n  # arity-1 literal, multiplied

    def fill(self):
        self.bodies[0] = Body(3)

    def total(self):
        t = 0
        for b in reversed(self.bodies):
            if b is not None:
                t += b.tag()
        return t


def main():
    r = Random()
    t = Tree(4)
    t.fill()
    print(r.seed, t.total())


main()
# NOT FAITHFUL TO bh (measured 2026-09-10). The two mechanisms that clear
# this file -- PYC_VIOLCS=2 (walk a violation back to the CreationSet that
# actually merges) plus PYC_CSMEMBER=1 (partition it by the members its
# creation points reach) -- take this from 1 diagnostic to 0 and leave `bh`
# at 10. On `bh` the member key fires on cs=1180 (8 defs -> 7 groups) but
# its remaining two-def merges decline with "1 group: every creation point
# on the same assign sets", so the partition it needs is still unnamed.
# Keep this as the small guard for the shape; `bh` still needs its own.
