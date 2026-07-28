# Minimal repro for the empty-container element-inference residual
# (ifa/issues/072 / the 043 family). Reading an ELEMENT out of a
# container that is allocated empty and NEVER written (`x = []`, then
# `x[0]`) has no element type to give the read, so FA reports the read
# as NOTYPE ("expression has no type") and codegen degrades it to a
# runtime trap.
#
# This is compile-only on purpose (no .exec.check): the read would trap
# at runtime, and CPython IndexErrors on the same line -- it is a
# MUTUAL-error case. That is the point of the repro: every *valid*
# CPython shape (a guarded `if lst: lst[0]`, `for x in lst`, `sum(lst)`,
# even a `len(x)>0`-guarded read) already types and runs correctly today
# -- forward, write-driven inference handles them. The only residual is
# reading past a provably-empty container, which no valid program does,
# so the honest fix is a clean codegen trap (043 option 1), not element
# seeding (a fixed default regresses the ops that already handle a
# bottom element -- see ifa/issues/072's negative-result writeup).
#
# When that residual is addressed the diagnostics below change; refresh
# this .check then (cf. tests/cross_type_method.py.check).
x = []
print(x[0])
