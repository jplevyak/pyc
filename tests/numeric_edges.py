# Edge values for int and float — exercises CG2_CAST and the
# printf-format dispatch that current tests don't hit.
# Catches: i64-overflow rounding bugs, sign-extension errors on
# the min/max int64 boundaries, hex/bin negative formatting.

# int64 max — both as a literal and as a runtime sum (the latter
# defeats constant-folding so the printf path actually runs on
# i64 SEXT'd input).  Skip the int64 min literal — it triggers a
# C front-end warning ("integer constant is so large it is
# unsigned"), the same shape as -9223372036854775808 in raw C.
print(9223372036854775807)         # 2**63 - 1, max int64
a = 9223372036854775806
print(a + 1)                       # same value, computed at runtime

# Max negative magnitude that fits cleanly as a signed int64.
b = 9223372036854775807
print(-b)                          # -(2**63 - 1)

# Negative values in bin / hex — exercises sign handling in the
# library str helpers (__pyc__/05_builtins.py's bin/hex), which
# v1 historically got wrong on -1 (looped over two's complement).
print(bin(-1))
print(bin(0))
print(bin(1))
print(hex(0))
print(hex(255))
print(hex(-1))

# Largest int32 representable.
print(2147483647)                  # 2**31 - 1
print(-2147483648)                 # -2**31

# Float values that DON'T require overflow-folding constants.
print(1.0)
print(-1.0)
print(0.0)
print(0.5)
print(-0.5)
