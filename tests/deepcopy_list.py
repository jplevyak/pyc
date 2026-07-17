# copy.deepcopy on (nested) lists actually deep-copies (pyc
# issues/025 R1 item 5): mutating the copy's nested containers must
# not touch the source. Requires recursive-ES splitting (each
# deepcopy recursion level gets a monomorphic contour -- see
# tests/recursive_polymorphic.py) plus the P_prim_copy scalar
# identity in both codegens for the int64 leaf contour.
import copy

a = [[1, 2], [3, 4]]
b = copy.deepcopy(a)
b[0][0] = 99
print(a)
print(b)

c = [1, 2, 3]
d = copy.deepcopy(c)
d[0] = 42
print(c)
print(d)

# Shallow copy.copy: one level only.
e = [7, 8]
f = copy.copy(e)
f[0] = 70
print(e)
print(f)
