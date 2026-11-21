# issues/188 item 1: a callback that raises. No exception may unwind into C:
# the trampoline prints it to stderr and the callback returns 0, and nothing
# reaches the caller -- CPython's behaviour. The expected output is pyc's:
# CPython's stderr also names the function at its address and prints the
# traceback, which pyc does not (stdout is identical).
from ctypes import CDLL, CFUNCTYPE, POINTER, sizeof, c_int, c_size_t

libc = CDLL(None)
CMP = CFUNCTYPE(c_int, POINTER(c_int), POINTER(c_int))
libc.qsort.argtypes = [POINTER(c_int), c_size_t, c_size_t, CMP]
libc.qsort.restype = None


def bad_cmp(a, b):
    raise ValueError("no order")


pair = (c_int * 2)(2, 1)
cb = CMP(bad_cmp)
libc.qsort(pair, 2, sizeof(c_int), cb)
print(pair[0], pair[1], "after")
