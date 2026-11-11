# pyc's `ctypes`: a static subset, lowered at compile time by
# python_ifa_ctypes.cc. A program's declarations (CDLL, argtypes, restype,
# Structure, POINTER) are consumed by that pass and never reach this module;
# the names below exist so that `from ctypes import c_int` binds. What does
# run is here: byref and pointer.
#
# A use the pass does not cover is refused at compile time with its line.

def byref(x):
    return x.__pyc_ffi_byref__()

def pointer(x):
    return x.__pyc_ffi_pointer__()

CDLL = 0
PyDLL = 0
cdll = 0
Structure = 0
POINTER = 0
c_bool = 0
c_byte = 0
c_ubyte = 0
c_short = 0
c_ushort = 0
c_int = 0
c_uint = 0
c_long = 0
c_ulong = 0
c_longlong = 0
c_ulonglong = 0
c_size_t = 0
c_ssize_t = 0
c_int8 = 0
c_uint8 = 0
c_int16 = 0
c_uint16 = 0
c_int32 = 0
c_uint32 = 0
c_int64 = 0
c_uint64 = 0
c_float = 0
c_double = 0
c_char_p = 0
c_void_p = 0
