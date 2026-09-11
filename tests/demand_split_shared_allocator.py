# ifa/133: a demand-driven call-site split, where the contour that OWNS the
# creation point has no choice to make and its CALLER does.
#
# `set(...)` allocates its backing list inside `set.__init__`, which has one
# in-edge (from `set.__new__`) -- so that contour cannot be partitioned. Both
# `set()` calls below reach ONE `set.__new__` contour, whose two in-edges are
# exactly these two lines. Under PYC_CSDCPA1=2 the two backing lists are
# therefore one CreationSet with ONE creation point, and its element unions
# int64 with str:
#
#     es=46  set.__init__  (owns the list creation point)   in_edges=1
#     es=45  set.__new__   (its only caller)                in_edges=2
#
# Route 4 declines "single creation point" -- correctly, there is nothing
# there to partition. ifa/129's third clause is the right mechanism but was
# pointed at the owning contour; it now CLIMBS to the nearest caller that has
# a choice, and because every edge there returns the same (still merged)
# contour, no type-shaped key can say which is which -- so the demand names
# the parts and the call site only labels them.
#
# The CSDEMAND line below is what this test pins: `climbs=1 hops=1` says the
# owning contour had no choice and its caller did; `named>=1` says the demand
# had to name the parts itself. That is the shape sudoku5 needs, in five
# lines -- see ifa/133. On THIS program the analysis also reaches the right
# answer without the mechanism, by later splitting; sudoku5 is the case where
# that later splitting is not guaranteed to arrive, which is the whole point.
a = set([1, 2])
b = set(["x", "y"])
a.add(3)
b.add("z")
print(sorted(a), sorted(b))
