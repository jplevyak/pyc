# issues/048 family, third manifestation: a None-initialised field that
# later holds a FLOAT. The field's C type is _CG_void (a pointer), and
# unlike an int -- which casts to a pointer silently, compiles, and then
# aborts at dispatch (tests/none_int_field_pair.py) -- a double cannot,
# so this fails as a raw clang error with no pyc diagnostic at all:
#
#   error: cannot cast from type 'double' to pointer type '_CG_void'
#     ((_CG_ps10901)t3)->e12 = (_CG_void)2.5;
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
