# ifa/issues/074: minimal reproducer for the FA contour-growth bug -- the
# one that leaves go, linalg and plcfrs non-convergent. 13 lines, and the
# growth is dead linear: with the divergence guards off, ess climbs
# 136 -> 193 -> 249 -> 305 -> 364 at passes 20/40/60/80/101 (+2.8 per
# pass) and css tracks it (+3 per pass), for as long as it is allowed to
# run. It only ever demands TYPE_CONFL and SETTER -- the
# cascade-serialization signature the three corpus programs share.
#
# The trigger needs all THREE of deepcopy, recursion, and a NESTED
# container. Remove any one and it converges:
#
#   deepcopy + recursion + nested (this)      NO  -- p102, growing
#   manual element copy instead of deepcopy   yes -- p13
#   deepcopy, no recursion                    yes -- p23
#   deepcopy + recursion, flat list           yes -- p10
#
# The TARGET (ifa/issues/074): this program is monomorphic --
# total: list[list[float]] -> float and shrink: list[list[float]] ->
# list[list[float]] at every depth -- so the optimal contour count is
# ~20 and INDEPENDENT of recursion depth. FA currently produces 236 and
# climbing, with 189 distinct type keys, because list.__deepcopy__'s
# `r = []` mints a fresh CreationSet per contour and each new CS gives
# the caller a new type key. The defect is CreationSet identity, not
# contour splitting.
#
# `.env` turns the guards off so the property under test is FA's own
# fixed point rather than the stall guard's cutoff. The check file asserts
# CONVERGED=1; today pyc prints CONVERGED=0, which is what .known_issue
# records. Distilled from linalg.py's determinant/Minor pair.
import copy


def shrink(M):
    M1 = copy.deepcopy(M)
    del M1[0]
    return M1


def total(M):
    if len(M) == 0:
        return 0.0
    return M[0][0] + total(shrink(M))


print(total([[1.0], [2.0], [3.0]]))
