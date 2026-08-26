# ifa/issues/055 minimal repro (6 lines; the issue previously recorded
# that no isolated repro existed and only the 500-line plcfrs.py showed
# it). Two ingredients, both required:
#
#   1. two or more ELEMENT TYPES across the set-operation call sites,
#      which makes the single `r = set()` inside set.difference()
#      polymorphic -- it is one creation site shared by every caller;
#   2. a CHAIN -- feeding a difference RESULT back into difference --
#      which makes that shared site's element type an input to itself.
#
# Either alone converges (2 types + no chain: 32 passes; 1 type + chain:
# 13 passes). Together FA never reaches a fixed point and stops on the
# pass cap. Nothing here is specific to `-`/__sub__: `&`, `|` and a
# plain `.difference()` call reproduce identically.
ai = set([1, 2, 3]); bi = set([2]); ci = ai - bi
print(len(ci))
as_ = set(["x", "y"]); bs = set(["x"]); cs = as_ - bs
print(len(cs))
di = ci - bi
print(len(di))
