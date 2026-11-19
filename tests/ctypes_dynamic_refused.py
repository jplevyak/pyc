# issues/188: ctypes used outside the static subset is refused with its line,
# not compiled into something that fails obscurely.
import ctypes
p = ctypes.cast(None, ctypes.c_void_p)
print(p)
