# issues/188: ctypes used outside the static subset is refused with its line,
# not compiled into something that fails obscurely.
import ctypes
x = ctypes.c_int(5)
print(x.value)
