# ifa/issues/180: `**` had no LLVM emitter and was silently dropped, so
# every power printed 0 under -b (nbody printed 0.000000000 for every
# energy). int ** int must also be EXACT: both backends used to go
# through a double, so 3**39 lost its low digits. Operands come from
# len(sys.argv) so nothing constant-folds.
import sys
k = len(sys.argv)
x = 1.5 + k
print(x ** 2)
print(x ** 0.5)
print(x ** 3 == x * x * x)
n = 3 + k
print(n ** 2)
print(3 ** (38 + k))
print((-2) ** (2 + k))
print(0 ** (k - 1))
print(2 ** (k + 61))
