# issues/188: a c_void_p RESULT is `int` or `None` in CPython, which has no
# static representation. Permissive mode's PYC_FFI_VOIDP_INT accommodation
# returns the address as an int (NULL is 0) and warns; --strict refuses.
import ctypes
libc = ctypes.CDLL(None)
libc.malloc.argtypes = [ctypes.c_size_t]
libc.malloc.restype = ctypes.c_void_p
print(libc.malloc(8) != 0)
