# ifa/issues/030 fixpoint sub-bug: two creation sites of the same
# class with *swapped* polymorphic children. Before the
# make_closure_var positional-slot fix, the per-receiver EntrySet
# clones left one bound-method closure's fun slot bottom across
# re-analysis passes (the closure CS persists in the result AVar's
# cs_map while every pass clears AVar state; the re-derived flow
# landed in an orphan AVar keyed by a different Var), so
# closure_used never got set, remove_unused_closures() stripped the
# closure, and the result vars went void -- "expression has no
# type" warnings and a runtime matching-function abort.
class Node:
    def val(self): return 0

class N1(Node):
    def val(self): return 1

class N2(Node):
    def val(self): return 2

class Inner(Node):
    def __init__(self, l, r):
        self.l = l
        self.r = r
    def val(self):
        return self.l.val() + self.r.val()

x1 = Inner(N1(), N2())
print(x1.val())
x2 = Inner(N2(), N1())
print(x2.val())
