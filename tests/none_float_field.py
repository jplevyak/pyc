# issues/048 family, third manifestation: a None-initialised field that
# later holds a FLOAT. The field's C type is _CG_void (a pointer), and
# unlike an int -- which casts to a pointer silently, compiles, and then
# aborts at dispatch (tests/none_int_field_pair.py) -- a double cannot,
# so this used to fail as a raw clang error with no pyc diagnostic at all
# ("cannot cast from type 'double' to pointer type '_CG_void'"). Since
# 2026-08-16 it is guarded and named at both the store and the load:
#
#   warning: field 'a' has no single representation: cannot store a
#   '_CG_float64' into a '_CG_void' -- a None-with-scalar or mixed-basic
#   union (issues/048)
#
# It still cannot run; what changed is that it now fails the way the rest
# of the family does, and says which field.
#
# Same root cause: pyc has no representation for {None, scalar}. Compare
# tests/none_int_field_pair.py (compiles, aborts) and
# tests/branch_merged_scalar_union.py ({int,str}, same). {None, class}
# and {int, float} both work -- see the boundary map in issues/048.
class V:
    def __init__(self):
        self.a = None

v = V()
v.a = 2.5
print(v.a)
