# issues/188: a foreign function with no argtypes is refused. CPython would
# guess a signature from the arguments at run time; pyc needs the declaration.
import ctypes
libc = ctypes.CDLL(None)
print(libc.getpid())
