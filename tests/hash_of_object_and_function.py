# issues/118: `hash()` of a class instance, or of a function, used to
# abort at runtime.
#
# __hash__ was defined on str, bytes, numeric, list and tuple, but not on
# `object` -- so `hash(x)` for anything else compiled with one warning and
# then died with
#
#   runtime error: matching function not found
#
# `object.__hash__` returning id(self) is CPython's own default (identity
# hash), and it is what shedskin's `long pyobj::__hash__() { return
# (intptr_t)this; }` does too. It covers functions as well: a closure
# never reaches object's class hierarchy, but it does reach this dispatch.
#
# NB the matching fallback on `__pyc_any_type__` was tried and REVERTED.
# It looked right by analogy with __pyc_to_bool__ (and with shedskin's
# `hasher(void *)` specialization), but it turns `hash(x)` into a
# multi-candidate dispatch and breaks exactly the cases below -- the
# hazard 00_runtime.py already records for hypothetical __getitem__ /
# __len__ stubs. It also would not have helped what motivated it: an
# EMPTY container's element type in pyc is bottom ("has no type"), not the
# any type, so there is no receiver for a fallback to attach to. See
# issues/118.
class A:
    def __init__(self, v):
        self.v = v

a = A(1)
b = A(2)
print(hash(a) == hash(a), hash(a) == hash(b))

def f():
    return 1

print(hash(f) == hash(f))

# hashes that were already defined stay themselves, and equal values must
# still hash equal
print(hash("xy") == hash("x" + "y"), hash(7) == hash(7), hash((1, 2)) == hash((1, 2)))
