# issues/188: the static ctypes subset. A foreign function declared the way
# ctypes asks (CDLL, argtypes, restype) is a direct call in the compiled
# program; this file runs unchanged under CPython, which is where its
# expected output comes from.
import ctypes
import ctypes.util
from ctypes import c_double, c_int, c_char_p, c_size_t

libc = ctypes.CDLL(None)
libm = ctypes.CDLL(ctypes.util.find_library("m"))
libm.cbrt.argtypes = [c_double]
libm.cbrt.restype = c_double
libm.ldexp.argtypes = [c_double, c_int]
libm.ldexp.restype = c_double
libc.strlen.argtypes = [c_char_p]
libc.strlen.restype = c_size_t
libc.abs.argtypes = [c_int]
libc.getenv.argtypes = [c_char_p]
libc.getenv.restype = c_char_p
libc.toupper.argtypes = [c_int]
libc.toupper.restype = c_int
# Narrow C types: the shim narrows on the way in and widens on the way out.
libc.atoi.argtypes = [c_char_p]            # restype defaults to c_int
truth = ctypes.CDLL(None)                  # a second handle, its own signatures
truth.abs.argtypes = [c_int]
truth.abs.restype = ctypes.c_bool
libm.fabsf.argtypes = [ctypes.c_float]
libm.fabsf.restype = ctypes.c_float
libc.srand.argtypes = [ctypes.c_uint]
libc.srand.restype = None

print(libm.cbrt(27.0))
print(libm.cbrt(8))
print(libm.ldexp(1.5, 4))
print(libc.strlen(b"hello"))
print(libc.abs(-7))
print(libc.getenv(b"PYC_NO_SUCH_VAR") is None)
h = libc.getenv(b"PATH")
if h is not None:
    print(len(h) > 0)
up = libc.toupper
print(chr(up(ord("a"))))
def total(xs):
    t = 0.0
    for x in xs:
        t += libm.cbrt(x)
    return t
print(total([1.0, 8.0, 27.0]))
print(libc.atoi(b"-42"), libc.atoi(b"2147483647"))
print(truth.abs(-1), truth.abs(0), libc.abs(-1))
print(libm.fabsf(-1.5), libm.fabsf(True))
print(libc.srand(7))
