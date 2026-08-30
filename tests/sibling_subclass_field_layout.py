# issues/121: two SIBLING SUBCLASSES of one base get their inherited
# fields at DIFFERENT struct slots, so a union receiver reads them
# through the wrong layout. Silent: zero warnings, exit 0, wrong output.
#
# S1 and S2 declare no fields of their own -- `a`, `b`, `c` are all
# assigned in Base.__init__. pyc prints S2's back REVERSED:
#
#     cpython: 11 11 12 13 1 | 21 21 22 23 2
#     pyc    : 11 11 12 13 1 | 23 23 22 21 2
#
# Cause: a field assigned only through a base's __init__ is not in the
# class's AST `has` list at all -- it is added during flow analysis by
# promote_field (python_ifa_sym.cc), which APPENDS in the order the
# fields are discovered, and that order comes from a hash set. The
# `has` index IS the emitted struct's eN suffix, so S1 got (a,b,c) at
# e15..e17 and S2 got (c,b,a). Every access casts the union receiver to
# ONE struct type, so S2's reads land on the wrong members.
#
# determine_layouts (clone.cc) already walks fields NAME-SORTED to
# compute the byte offsets that prim_period_offset validates -- so the
# "mismatched offsets" check passed on an ordering the emitter never
# used. That is why this produces no diagnostic.
#
# This is richards' miscompile (issues/120), and the reason its
# scheduler read `ident` as a neighbouring field's value.
class Base:
    def __init__(self, a, b, c):
        self.a = a
        self.b = b
        self.c = c
    def get(self):
        return self.a

class S1(Base):
    def __init__(self, a, b, c):
        Base.__init__(self, a, b, c)
    def fn(self):
        return 1

class S2(Base):
    def __init__(self, a, b, c):
        Base.__init__(self, a, b, c)
    def fn(self):
        return 2

xs = [S1(11, 12, 13), S2(21, 22, 23)]
for x in xs:
    print(x.get(), x.a, x.b, x.c, x.fn())
