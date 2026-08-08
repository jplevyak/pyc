# ifa/issues/081: `int OP bool` (int operand first) used to crash the
# compiler itself -- FA's numeric constant-fold (fa.cc's
# add_send_edges_pnode) intersected each operand's type against
# fa->type_world.anynum_kind before folding, and `bool` was never a
# specializer of that lattice, so the intersection came back empty and
# the very next line indexed off the end of an empty AType. Fixed two
# ways: (1) ifa itself now guards against an empty fold result instead
# of indexing it unconditionally (crash-avoidance, applies regardless
# of language), and (2) pyc's own callbacks (PycCallbacks::bool_is_numeric,
# python_ifa.h) opt `bool` into ifa's numeric lattice (ifa.h,
# IFACallbacks::bool_is_numeric), matching Python's actual
# `isinstance(True, int) is True`, so these now fold to a real, typed
# int result instead of merely avoiding the crash.

n = 5
b = True
f = False
print(n * b)   # 5
print(n * f)   # 0
print(n + b)   # 6
print(n - b)   # 4
print(n // b)  # 5
print(n % b)   # 0
print(n & b)   # 1
print(n | b)   # 5
