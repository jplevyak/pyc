# ifa/issues/050 direction 3b: a global's value is the whole-program
# union of every write to it, with no call-graph precision.
#
# FA's constant propagation lives in the TYPE LATTICE and it works well:
# a Var whose value FA can resolve becomes the canonical constant Sym,
# unreachable arms do not contribute (verified: `c = 0; if c: x = "str"
# else: x = 5; print(x + 1)` compiles and prints 6, with the str arm
# contributing nothing), and folds CASCADE inside FA's own worklist (the
# same program with an intermediate `d` folds twice and still types).
# That is sparse conditional constant propagation, in the lattice.
#
# What it does not have is a per-contour value for a GLOBAL. issues/031
# gave each global READ its own EntrySet-contoured temp, but the value
# that temp loads is still flow-insensitive: every write to the cell,
# unioned, regardless of which ones can reach this read.
#
# Below, `g` is written twice -- `0` at module level and `"five"` in
# setup() -- and read once, in use(), which runs only after setup(). The
# call graph proves the read sees a str. FA types it {int64, str} and the
# program is refused:
#
#     fail: program does not type
#
# CPython prints 4.
#
# Not a boxing issue (issues/018): there is no valid program state in
# which `g` holds both. It is the missing interprocedural slot promotion
# 050 calls 3b -- "interprocedural mem2reg for a scalar slot".
g = 0

def setup():
    global g
    g = "five"

def use():
    return len(g)

setup()
print(use())
