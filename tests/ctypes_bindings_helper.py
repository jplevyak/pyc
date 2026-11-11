# issues/188: a bindings module, in the shape a generator emits: the library
# handle and one module-level name per foreign function. Imported by
# ctypes_bindings.py.
import ctypes
import ctypes.util
from ctypes import c_double, c_int, c_char_p, POINTER, Structure

_m = ctypes.CDLL(ctypes.util.find_library("m"))
_c = ctypes.CDLL(None)

class DivT(Structure):
    _fields_ = [("quot", c_int), ("rem", c_int)]

hypot = _m.hypot
hypot.argtypes = [c_double, c_double]
hypot.restype = c_double

div = _c.div
div.argtypes = [c_int, c_int]
div.restype = DivT

_c.strlen.argtypes = [c_char_p]
_c.strlen.restype = ctypes.c_size_t
lib = _c
