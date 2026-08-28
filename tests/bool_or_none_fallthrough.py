# ifa/issues/118 minimal repro for the BOXING refusal, reduced from
# shedskin_examples/chess (377 lines -> 7) by delta-debugging.
#
# `f` returns False on one path and falls off the end -- implicit None --
# on another, so its result is {bool, None}. bool is 1 byte and None is
# pointer-sized, and pyc does not box, so the slot has no representation:
#
#   mismatched field members: bool(1) __pyc_None_type__(8)
#   fail: mismatched field sizes: class 'closure' field '<anon>'
#         mixes 1- and 8-byte members ('__pyc_None_type__')
#
# This is the same shape chess hits: its `rowAttack` returns False, or a
# bool, or falls off the end of its loop, and that result flows through
# the `nonpawnWhiteAttacks` lambda's closure slot.
#
# {None, int64} already WORKS -- an integer and a pointer round-trip
# bit-for-bit -- so this fails only on WIDTH. Widening the slot is
# necessary but not sufficient; see the issue for the measurement
# (PYC_WIDEN_UNION_FIELD gets past the check and into 18 C errors).
#
# The LOOP is load-bearing: the same function written with a plain `if`
# and no loop compiles, because the implicit-None fall-through is then
# collapsed rather than surviving as a separate path.
def f(xs):
    for k in xs:
        if k:
            return False
    # falls off the end -> None

print(f([0, 1]))
