# ifa/issues/144: two `[]` literals whose elements are different basic
# types must not share a CreationSet.
#
# Under PYC_CSDCPA1 every list starts on ONE CreationSet, so separating
# these two is entirely the demand splitter's job. `list.append` already
# gets two contours here by ordinary argument-type CPA -- measured,
# `[list#983][int64]` and `[list#983][str]` -- so the receiver's contour is
# NOT what needs keying; the CreationSet simply has to split, and the
# append contours follow.
#
# This is the repro that caught route 4's partition (ifa/144 fix 1) keying
# its group signature on `cs->defs` while the CSFlowGraph's creation points
# live in a DIFFERENT AVar set. When the two are disjoint -- the CSFLOW
# probe prints `in_defs=0` -- every def scored an empty signature, they
# collapsed into one group, and the rung declined having learned nothing.
# The three assign sets it declined on separate `int64` from `str`
# perfectly:
#
#   CSFLOW cs=983 defs=6 sets=3 csites=2 (in_defs=0)
#     set[0] type= int64 str   cps=2
#     set[1] type= int64       cps=1
#     set[2] type= str         cps=1
#
# Failure mode when it regresses: `expression has mixed basic types:
# ( int64 str )` and no binary.
#
# STATUS 2026-09-08: passes at the DEFAULT, FAILS on the flag arm
# (`PYC_CSDCPA1=2 PYC_CSLADDER=3`). It is ifa/146 B's minimal repro -- five
# lines, and it exhibits the exact plumbing gap that blocks the flip:
#
#   CSFLOW p=3 cs=983 sym=list defs=6 sets=3 csites=2 (in_defs=0) empty=6
#
# THREE assign sets, so the demand names its parts perfectly (int64, str,
# and their union). Two creation points. But `in_defs=0`: the flow graph's
# creation points and `cs->defs` are DISJOINT AVar sets, so the partition
# the demand names cannot be applied to the defs that need re-pointing.
# `split_css_by_defs` detects this ("informative") and declines.
#
# It passed on the flag arm until ifa/146 C removed route 4's fan, which had
# been covering the decline. That was a real trade and the gates did not
# catch it, because they run at the DEFAULT only -- worth remembering when
# judging a flag-arm change by the suite.
a = []
a.append(1)
b = []
b.append("x")
print(a[0], b[0])
