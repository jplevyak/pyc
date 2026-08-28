# ifa/issues/105: four nested copy.deepcopy calls used to be a BOXING
# refusal; three were fine. Fixed 2026-08-28 by PYC_CSMOLD mode 3.
#
# `copy.deepcopy` of a list-of-lists enters `list.__deepcopy__` (whose
# body is `r = []` + `r.append(self[k].__deepcopy__())`) once per nesting
# level, and FA splits a contour per level. The MOLD FALLBACK
# (ifa/issues/101, creation_point) then handed every split child the same
# container instance -- it reuses any CreationSet whose `creation_var`
# matches, with no regard for contour -- so all those contours shared one
# `r`, and `r`'s element type came back as
#
#     { list<itself>, int64, int64, list, int64 }
#
# both self-referential and container/scalar mixed. That is
# unrepresentable without boxing, so codegen refused:
#
#     fail: a variable holding 'int64' has no representation: '__add__'
#     resolved to the CONTAINER method, whose receiver may be a scalar
#
# The fallback was silently undoing PYC_CSSPLIT (ifa/issues/055, default
# since a day earlier), whose entire point is that "the CreationSet
# follows the EntrySet split". Every csmold hit in this program's run had
# a non-null `es->split`. Mode 3 declines the mold for a split child, and
# each level gets its own monomorphic list again.
#
# The 3-vs-4 cliff is what made this findable: with three copies every
# list CreationSet is clean (`elem=[list]` / `elem=[int64]`), with four
# two of them go self-referential.
#
# NOT the same as tests/deepcopy_recursive_nested_growth.py
# (ifa/issues/074), which is still a `.known_issue`: there the chain is
# UNBOUNDED -- a self-recursive function deep-copies its own copy -- so
# declining the mold only makes FA unroll a fresh contour per level until
# the pass limit. That one needs a terminating fixed point (recognising
# that copy(list<int>) IS list<int>), not a better sharing rule, and this
# fix leaves its CONVERGED=0 exactly where it was. Nor is it
# tests/deepcopy_tuple_copy_of_copy.py (issues/112), which is
# tuple-specific and shows up at TWO copies.
import copy

m = [[1, 2], [3, 4]]
a1 = copy.deepcopy(m)
a2 = copy.deepcopy(a1)
a3 = copy.deepcopy(a2)
a4 = copy.deepcopy(a3)
print(a4[0][0] + 1)
