# ifa/issues/105: THE FAILURE repro -- three lines that produce
# shedskin_examples/plcfrs's exact compile failure:
#
#   internal: sizeof_element of non-container type '...' (in __add__)
#   -- FA specialized a container method against a scalar
#
# A single variable holds {list, float}, and `+` then resolves to a
# CONTAINER method whose receiver may be a scalar. CPython prints 7.0.
#
# The essential ingredient is that the union forms in ONE VARIABLE. The
# same two types reaching `__add__` through SEPARATE call sites --
#
#     def add(a, b): return a + b
#     add([1, 2], [3]); add(3.5, 1.5)
#
# -- compiles and runs fine, because FA gives those calls separate
# contours. So this is not "pyc cannot mix list and float"; it is the
# branch merge defeating contour separation, which is the 018/101 pair.
import sys
x = [1] if len(sys.argv) > 1 else 3.5
print(x + x)
