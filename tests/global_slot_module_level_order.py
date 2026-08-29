# ifa/issues/050 direction 3b, the SIMPLEST case: two writes to a module
# global, in order, in the SAME function, with the read after both.
#
#     g = 0
#     g = "five"
#     print(len(g))
#
# Both print 4. pyc used to refuse with `program does not type`, because a
# module-level data cell lives in one shared GLOBAL_CONTOUR AVar (fa.h's
# GLOBAL_CONTOUR note), so every read saw the union of every write
# anywhere -- here {int64, str}.
#
# FIXED by 050 stage 1: IFACallbacks::provably_constant_load, consulted
# from FA's Code_MOVE transfer, resolves the load to the nearest
# dominating store when EVERY store to that cell program-wide is in the
# same function and dominates the load. Skipping the normal flow edge is
# what then leaves the cell with no reader, so the consumer-aware BOXING
# check (fa.cc) no longer diagnoses the slot either -- both halves are
# needed, and the second is why a fold alone was not enough.
#
# This is the same defect as tests/global_slot_call_graph_precision.py but
# without any interprocedural component at all: no call graph is needed to
# know that the second write kills the first, only the ordering the
# function's own PNode graph already has. issues/031 step 2 gave each
# global READ an EntrySet-contoured temp; the WRITES were left sharing one
# cell, which is the half that makes this fail.
#
# tests/global_slot_call_graph_precision.py is the interprocedural case
# and stays a known issue: its store is in another function, which this
# rule declines by construction. That needs 050 stage 2 (a per-Fun mod-set
# over the call graph) and stage 3 (the per-ES summary).
#
# NOT issues/018 boxing: no valid program state has `g` holding both.
g = 0
g = "five"
print(len(g))
