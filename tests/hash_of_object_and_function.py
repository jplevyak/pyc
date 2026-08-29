# issues/118: `hash()` of a class instance, or of a function, used to
# abort at runtime.
#
# __hash__ was defined on str, bytes, numeric, list and tuple, but not on
# `object` -- so `hash(x)` for anything else compiled with one warning and
# then died with
#
#   runtime error: matching function not found
#
# An identity hash returning id(self) is CPython's own default, and is
# what shedskin's `long pyobj::__hash__() { return (intptr_t)this; }` does
# too.
#
# It lives on `__pyc_any_type__`, the top type -- NOT on `object`, where
# it was first written. Two reasons, both measured: a closure never
# reaches object's class hierarchy, so `hash(f)` below still failed with
# it there; and defining it on BOTH turns hash() into a multi-candidate
# dispatch that aborts even on a plain instance. One definition, on the
# top type, covers instances and functions alike.
#
# It does NOT make an empty container hashable, which is what motivated
# adding it: pyc types an empty container's element as bottom ("has no
# type"), not as the any type, so there is no receiver for a top-type
# method to attach to. See issues/118.
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
