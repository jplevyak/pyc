# issues/018, the shape that is still broken: a plain scalar merged from
# two branches, no container anywhere. `cond` is read from sys.argv so FA
# cannot constant-fold the branch away.
#
# pyc emits 8 warnings, compiles, and the binary aborts with "matching
# function not found"; CPython prints "hi world". Same failure mode as
# issues/048 (a None|int field pair) -- a union of BASIC types reaching an
# ordinary operation.
#
# 018's container shapes (two dicts with different key types, sets, mixed
# values, object keys) are all fixed and are pinned by
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
