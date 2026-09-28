# ifa/176: a deep copy fed back into its own source (the global M) used to
# fuse the outer and inner copies into one CreationSet whose element was
# {float, list}; CPython prints 1.0.
import copy
M = [[1.0], [2.0]]
M = copy.deepcopy(M)
print(M[0][0])
M = copy.deepcopy(M)
print(M[1][0] + M[0][0])
