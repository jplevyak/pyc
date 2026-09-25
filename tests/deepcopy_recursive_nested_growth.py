# ifa/issues/074: minimal reproducer for FA non-convergence, distilled from
# linalg.py's determinant/Minor pair. The analysis never reaches a fixed
# point: contours keep being added until the pass cap, and the final types
# are wrong as well -- `M[0][0]` sees a `list` where only `float` belongs.
# Recursion and the nested list are both needed: the flat-list and the
# non-recursive versions converge. deepcopy is not -- copying the rows by
# hand does not converge either.
#
# The target: the program is monomorphic -- total: list[list[float]] ->
# float and shrink: list[list[float]] -> list[list[float]] at every depth
# -- so the contour count should be small and independent of recursion
# depth.
#
# `.env` turns the stall guards off, because a guard cutoff also reports
# CONVERGED=0; the property under test is FA's own fixed point. `.check`
# asserts CONVERGED=1 and `.exec.check` CPython's output, so the test flips
# to PASS only when the analysis both converges and types the program.
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
