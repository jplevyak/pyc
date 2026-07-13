# Issue 033 S3 sketch (e): data-polymorphism CS-split churn. One
# creation point (inside make) yields ONE CreationSet holding both
# a's and b's lists; fill/append write int through one and float
# through the other, so the analysis must split the CS by setter
# equivalence (split_css) to keep the element types apart -- ES
# splitting alone cannot separate defs of a single creation site.
# Locks: the split happens and stays precise (drain(a) stays int,
# drain(b) stays float, verified against CPython). The issue 033 D5
# record-only CS ledger observes the decision (recorded once,
# cs_dups 0 in the -v PASS line); cross-pass re-derivation showing
# up here would mean CS split persistence via av->cs_map regressed.


def make():
    return []

def fill(l, v):
    l.append(v)

def drain(l):
    return l[0]

a = make()
b = make()
fill(a, 1)
fill(a, 2)
fill(b, 1.5)
fill(b, 2.5)
print(drain(a) + 1)
print(drain(b) + 1.5)
