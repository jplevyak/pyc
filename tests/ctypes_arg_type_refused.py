# issues/188: a wrong argument type for a declared foreign function is a
# compile error, where CPython raises ctypes.ArgumentError at the call.
import ctypes
libc = ctypes.CDLL(None)
libc.abs.argtypes = [ctypes.c_int]
print(libc.abs(1.5))
