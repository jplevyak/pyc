# ifa/200 A6: an int stored into a float64 list element. The LLVM backend
# stored at the VALUE's type, so the int's bits landed in a double slot:
# `m[1][1] = 1` read back as 5e-324, which zeroed every one of softrender's
# matrices (init_perspective writes `m[3][2] = 1`). C converts by
# assignment, as CPython's float list simply holds the int.
# `+ 0.5` keeps the permissive int->float widening out of the output.
import sys

m = [[0.0 for i in range(2)] for j in range(2)]
m[0][0] = 1
m[1][1] = len(sys.argv) + 2
r = m[0]
r[1] = 7
print(m[0][0] + 0.5, m[1][1] + 0.5, m[0][1] + 0.5)
