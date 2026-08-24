# issues/018: a plain scalar merged from two branches, no container
# anywhere. `cond` is read from sys.argv so FA cannot constant-fold the
# branch away.
#
# EXPECTED: a compile ERROR naming the variable and its union, and a
# non-zero exit (.check_fail + .check). `x` holds {int64, str}, which
# has NO RUNTIME REPRESENTATION, and boxing is deliberately not an
# option in this project -- so refusing is the only honest answer and
# this test pins that refusal.
#
# It used to be a `.known_issue`: 8 warnings, exit 0, and a binary that
# aborted with "matching function not found" the moment `x` was used.
# That was the worst available outcome. shedskin reaches the same wall
# and at least fails at build time (`invalid conversion from
# '__ss_int' to 'pyobj*'`), which is what this now matches.
#
# 018's container shapes (two dicts with different key types, sets,
# mixed values, object keys) are fixed and pinned by
# tests/dict_mixed_key_types.py.
import sys

cond = len(sys.argv) > 1
if cond:
    x = 5
else:
    x = "hi"
if cond:
    print(x + 10)
else:
    print(x + " world")
