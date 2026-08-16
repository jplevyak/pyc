# ifa/issues/074: the TYPE_CONFLUENCE splitter (stage 1) -- the one that
# separates a contour whose formal receives two different types.
#
# `ident` is called with an int and a str, so its single contour is a type
# confluence and stage 1 splits it. This is the baseline of the
# tests/splitter_*.py set: it fires on essentially every program, so a run
# in which it does NOT appear means something has gone quite wrong.
def ident(x):
    return x

print(ident(1), ident("a"))
