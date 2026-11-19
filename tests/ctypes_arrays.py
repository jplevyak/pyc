# issues/188 item 3: arrays (c_int * 4), a Structure and an array held by
# value in a field, and c_char * N. Every line is CPython's behaviour.
from ctypes import CDLL, POINTER, Structure, sizeof
from ctypes import c_int, c_double, c_char, c_size_t

N = 3
A = c_int * 4
libc = CDLL(None)
libc.memset.argtypes = [POINTER(c_int), c_int, c_size_t]
libc.memset.restype = None
libc.strlen.argtypes = [POINTER(c_char)]
libc.strlen.restype = c_size_t

# Construction: the items given, the rest zero.
a = A(1, 2)
print(len(a), a[0], a[1], a[3], list(a), [x * 2 for x in a])
a[3] = 9
print(a[3], a[-1], sum(a))
print(list((c_double * 2)(1.5)))

# An array converts to POINTER(elem): memset writes it.
libc.memset(a, 0, 8)
print(list(a))
b = A(5, 6, 7, 8)
libc.memset(b, 0, 4)
print(list(b))


class S(Structure):
    _fields_ = [("q", c_int)]


# An array of Structures: a[i] is a view, a[i] = s a copy.
SA = S * 2
sa = SA()
sa[1].q = 5
v = sa[1]
v.q = 6
print(sa[1].q)
s0 = S(7)
sa[0] = s0
s0.q = 70
print(sa[0].q, s0.q)


class T(Structure):
    _fields_ = [("v", c_int * N), ("s", S), ("name", c_char * 8)]


t = T()
print(list(t.v), t.s.q, t.name)
t.v[1] = 4                           # a view of the field: writes t
print(list(t.v))
a2 = t.s
a2.q = 9
print(t.s.q)
t.s = S(3)                           # a copy into the field; a2 still views it
print(t.s.q, a2.q)
t.v = (c_int * N)(7, 8, 9)
print(list(t.v))
t.name = b"hi"
print(t.name, len(t.name))
t.name = b"abcdefgh"                 # exactly full: no NUL
print(t.name)
t.name = b"xy"
print(t.name)
u = T(s=S(4), name=b"u")
print(u.s.q, u.name, list(u.v))

# c_char * N: .value to the first NUL, .raw all of it.
cb = (c_char * 5)()
cb.value = b"ab"
print(cb.value, cb.raw, cb[0], len(cb))
print(libc.strlen(cb))
cb.raw = b"xyz"
print(cb.raw)

# Arrays of arrays.
M = (c_int * 2) * 3
m = M()
m[2][1] = 5
print([list(r) for r in m])

print(sizeof(A), sizeof(T), sizeof(a), sizeof(M), sizeof(cb))
