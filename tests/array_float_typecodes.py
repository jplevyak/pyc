# array: the float typecodes f and d (issues/214), next to an integer one,
# through both import forms. 'f' rounds each value to binary32 as CPython's
# C float does; tobytes/frombytes are the IEEE bytes, little-endian.
import array as arraymod
from array import array

a = array('f', [0.1, 1e40, 2.0])
print(array('f', [2, 3]), array('d', (7, 8)))
print(a, a[0], a.itemsize, a.tobytes())
d = arraymod.array('d', [1.0, 2.5])
print(d, repr(d), str(d), d.itemsize, d.tobytes())
print(array('d'), array('B', [1, 2]), array('l').itemsize, array('L').itemsize, array('i').itemsize, array('q').itemsize)
e = array('d')
e.frombytes(d.tobytes())
print(e, e[1:], len(e), list(e), 2.5 in e, 9.0 in e)
e.append(3)
e.extend([4, 5])
e.extend([0.5])
e[0] = 7
print(e, e.tolist(), e[-1])
del e[1]
print(e, e[0] + e[1])
g = array('f')
g.fromlist([1.1, 2.2])
g.fromlist([3])
g.frombytes(array('f', [3.3]).tobytes())
print(g, [x * 2 for x in g])
q = array('q', [-1, 2 ** 62])
r = array('q')
r.frombytes(q.tobytes())
print(r, array('l', [-5]).tobytes())
try:
    array('z')
except ValueError as x:
    print(x)
try:
    d.append('x')
except TypeError as x:
    print(x)
