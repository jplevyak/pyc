# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# pystone prints only "This machine benchmarks at N pystones/second", a
# rate. The benchmark's deterministic result is the global state Proc0
# leaves behind; this runs it for a fixed number of loops and prints that.
# corpus_sweep.sh runs this in place of pystone.py in run/check mode.
import pystone

pystone.Proc0(100000)
print("IntGlob", pystone.IntGlob, "BoolGlob", pystone.BoolGlob)
print("Char1Glob", repr(pystone.Char1Glob), "Char2Glob", repr(pystone.Char2Glob))
print("Array1Glob[8]", pystone.Array1Glob[8], "Array2Glob[8][7]", pystone.Array2Glob[8][7])
r = pystone.PtrGlb
print("PtrGlb", r.Discr, r.EnumComp, r.IntComp, repr(r.StringComp))
n = pystone.PtrGlbNext
print("PtrGlbNext", n.Discr, n.EnumComp, n.IntComp, repr(n.StringComp))
