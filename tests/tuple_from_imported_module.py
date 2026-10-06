# A heterogeneous tuple built only in an IMPORTED module. The unrolled
# tuple methods (__str__, __contains__, ...) are sized by a scan that
# used to see only the main file, so printing this was refused.
import tuple_from_imported_module_helper as h

t = h.rec()
print(t)
print("a" in t, 2.5 in t, "z" in t)
w = h.wide()
print(w)
print(w.count(4), w.index("c"))
