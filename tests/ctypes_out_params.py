# issues/188 item 2: scalar boxes (c_int(5), .value), out-parameters through
# byref, POINTER(scalar) as an argument, a result and a field, c_char, and
# sizeof. Every line is CPython's behaviour.
import ctypes
from ctypes import CDLL, POINTER, Structure, byref, pointer, sizeof
from ctypes import c_int, c_int32, c_long, c_double, c_char, c_char_p, c_ubyte, c_bool

libm = CDLL("libm.so.6")
libc = CDLL(None)
libm.frexp.argtypes = [c_double, POINTER(c_int)]
libm.frexp.restype = c_double
libm.modf.argtypes = [c_double, POINTER(c_double)]
libm.modf.restype = c_double
libc.strtol.argtypes = [c_char_p, POINTER(c_char_p), c_int]
libc.strtol.restype = c_long
libc.abs.argtypes = [c_int]
libc.abs.restype = c_int
libc.toupper.argtypes = [c_char]
libc.toupper.restype = c_char
IntP = POINTER(c_int)
libc.memcpy.argtypes = [IntP, IntP, ctypes.c_size_t]
libc.memcpy.restype = IntP

# Out-parameters.
e = c_int()
print(libm.frexp(12.0, byref(e)), e.value)
ip = c_double()
print(libm.modf(3.25, byref(ip)), ip.value)
end = c_char_p()
print(libc.strtol(b"123abc", byref(end), 10), end.value)

# A box passed by value where its own class is declared; c_int32 is c_int.
print(libc.abs(c_int(-7)), libc.abs(c_int32(-8)), libc.abs(-9))

# Boxes: construction, .value, narrowing to the C type, repr.
x = c_int(5)
x.value = x.value + 1
print(x.value, x, repr(c_double(2)), c_bool(3).value)
b = c_ubyte(300)
print(b.value)
b.value = -1
print(b.value)
print(c_char(b"a").value, c_char(66).value, c_char().value, c_char_p().value, c_char_p(b"hi").value)
print(libc.toupper(b"q"))

# POINTER(c_int): p[i] is the value, p.contents a box over the same int.
a = c_int(7)
p = pointer(a)
print(isinstance(p[0], int), p[0], p.contents.value)
p[0] = 9
print(a.value)
p.contents.value = 11
print(a.value)
p.contents = c_int(4)                  # repoints p; a is unchanged
print(a.value, p[0])
q = IntP()
print(bool(q), bool(p))
try:                                   # (q[0] raises the same in CPython; a raise
    print(q.contents.value)            # through a subscript waits on issues/175)
except ValueError as ex:
    print("ValueError", ex)

# A POINTER(c_int) result, and memcpy through two pointers.
src = c_int(42)
dst = c_int(0)
r = libc.memcpy(pointer(dst), byref(src), sizeof(c_int))
print(dst.value, r[0], r.contents.value)


# A POINTER(c_int) field, and a c_char field.
class Rec(Structure):
    _fields_ = [("tag", c_char), ("count", POINTER(c_int))]


n = c_int(3)
rec = Rec(b"z", pointer(n))
rec.count[0] += 10
print(rec.tag, n.value, rec.count.contents.value)
rec.tag = 65
print(rec.tag)

# sizeof of a type and of an instance.
print(sizeof(c_int), sizeof(c_double), sizeof(c_char), sizeof(IntP), sizeof(Rec), sizeof(x), sizeof(rec))
