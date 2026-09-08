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
a = []
a.append(1)
b = []
b.append("x")
print(a[0], b[0])
