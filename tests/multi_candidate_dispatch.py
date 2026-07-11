# Regression for issue 033 S5 M1: pattern matcher used to enumerate the
# full Cartesian product of per-argument CreationSets when MULTIPLE
# candidate functions were live (single-candidate calls were already
# collapsed by issue 037; multi-candidate calls kept full enumeration).
# Fixed by extending dispatch-equivalence collapsing to K live
# candidates, keyed on each candidate's per-position accept/exact/this
# verdict plus cs->sym->type (and cs->sym itself when the candidates'
# formal dispatch types differ at that position).
#
# Three receiver types with an identical method signature, called
# through a polymorphic list so the send is genuinely multi-candidate;
# each argument position carries a widening union (constants of two
# different types) to exercise the collapsing across arity.
class A:
    def go(self, x, y, z, w): return 1

class B:
    def go(self, x, y, z, w): return 2

class C:
    def go(self, x, y, z, w): return 3

objs = [A(), B(), C()]
for o in objs:
    print(o.go(1, 2, 3, 4))
    print(o.go(1.5, 2.5, 3.5, 4.5))
    print(o.go(1, 2.5, "s", None))
    print(o.go(2, 3.5, "t", None))
