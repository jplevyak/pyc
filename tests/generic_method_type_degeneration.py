# ifa/issues/105: minimal reproducer for TYPE DEGENERATION in shared
# generic container methods.
#
# This program is CORRECT under pyc today -- it compiles, converges in 7
# passes with 0 violations, and matches CPython. What it pins is a
# PRECISION property, invisible in the output: with `IFA_DBG_DEGEN=3` pyc
# reports **74 degenerate AVars** here (an AVar whose type spans >= 3
# distinct syms), against **0** for hello-world and 0 for the same program
# with only two element types.
#
# The engine is `list.__add__`'s local accumulator in
# __pyc__/04_sequence.py:
#
#     if isinstance(l, tuple):
#       r = []
#       for k in range(len(self)): r.append(self[k])
#       for k in range(len(l)):    r.append(l[k])
#
# `r` merges elements from BOTH operands, so unless FA gives `__add__` a
# separate contour per element type, `r` becomes the union of every list
# ever concatenated in the program. The degenerate sites reported here are
# at exactly the __pyc__ lines shedskin_examples/plcfrs degenerates at
# (`__add__` 1008, `__getitem__` 905/906), where the same effect reaches
# 2232 violations and a compile failure.
#
# Six element types is the threshold that makes it appear; two does not.
# Keep this test PASSING -- its value is as a measurement anchor for any
# fix to 101/018, not as a failure to be repaired.

# many element types through the same generic container methods,
# plus comparison/sorting -- plcfrs's __lt__/__eq__/__add__ mix
class Item:
    def __init__(self, k): self.k = k
    def __lt__(self, o): return self.k < o.k
class Edge:
    def __init__(self, w): self.w = w
    def __lt__(self, o): return self.w < o.w

ints  = [3, 1, 2] + [4]
strs  = ["b", "a"] + ["c"]
flts  = [2.5, 1.5] + [0.5]
items = [Item(2), Item(1)] + [Item(3)]
edges = [Edge(2.0), Edge(1.0)] + [Edge(3.0)]
nest  = [[1, 2], [3]] + [[4, 5]]

ints.sort(); strs.sort(); flts.sort(); items.sort(); edges.sort()
print(ints[0], strs[0], flts[0], items[0].k, edges[0].w, nest[0][0])
