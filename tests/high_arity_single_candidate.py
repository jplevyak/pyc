# Regression for issue 037: pattern matcher used to enumerate the full
# Cartesian product of per-argument CreationSets — 2^13 leaf evaluations
# per re-match for a 13-argument function with {const, base-type} unions.
# Fixed by dispatch-equivalence collapsing (single non-generic candidate
# path in find_best_matches).
def f(a, b, c, d, e, g, h, i, j, k, l, m, n): return a
x = f(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)    # all {const,int64} pairs
y = f(1.0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)  # perturb first arg to float
print(x)
print(y)
