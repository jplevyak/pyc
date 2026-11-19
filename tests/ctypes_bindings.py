# issues/188: foreign functions declared in one module and called from
# another, as module-level names, through the module, and through the
# library object itself.
import ctypes_bindings_helper
from ctypes_bindings_helper import hypot, div, lib

print(hypot(3.0, 4.0))
d = div(17, 5)
print(d.quot, d.rem)
print(ctypes_bindings_helper.lib.strlen(b"four"), lib.strlen(b""))
def lens(xs):
    return [lib.strlen(x) for x in xs]
print(lens([b"a", b"bc", b"def"]))
