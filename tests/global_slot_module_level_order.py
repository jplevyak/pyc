# ifa/issues/050 direction 3b, the SIMPLEST case: two writes to a module
# global, in order, in the SAME function, with the read after both.
#
#     g = 0
#     g = "five"
#     print(len(g))
#
# CPython prints 4. pyc refuses with `program does not type`, because a
# module-level data cell is not SSU-renamed: it lives in one shared
# GLOBAL_CONTOUR AVar (fa.h's GLOBAL_CONTOUR note), so every read sees the
# union of every write anywhere -- here {int64, str}.
#
# This is the same defect as tests/global_slot_call_graph_precision.py but
# without any interprocedural component at all: no call graph is needed to
# know that the second write kills the first, only the ordering the
# function's own PNode graph already has. issues/031 step 2 gave each
# global READ an EntrySet-contoured temp; the WRITES were left sharing one
# cell, which is the half that makes this fail.
#
# It is the first buildable step of 3b, and strictly smaller than the
# interprocedural summary that issue describes: SSU-rename the cell within
# a function, breaking the chain only at a call whose transitive mod-set
# contains it (computable exactly the way compute_fun_can_raise computes
# its boolean over Fun::calls).
#
# NOT issues/018 boxing: no valid program state has `g` holding both.
g = 0
g = "five"
print(len(g))
