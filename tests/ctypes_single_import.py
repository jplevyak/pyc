# issues/188: `from ctypes import NAME` with a single name is a ctypes import
# too. The scan used to read only a list of names, so this module was never
# lowered and c_int was pyc_lib's placeholder.
from ctypes import c_int
x = c_int(3)
x.value = x.value * 7
print(x.value, x)
