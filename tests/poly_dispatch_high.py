# Polymorphic dispatch: 6 concrete types at the call site.
#
# Inner.val() calls self.l.val() where l: N1|N2|N3|N4|N5|Inner (6 types).
# FA can't monomorphise because the tree is recursive.
# Expected codegen: indirect call through method-pointer slot (table dispatch).
#
# Tree construction forces Inner.l to cover all 6 types:
#   a = Inner(N1, N2)     l=N1
#   b = Inner(N2, N1)     l=N2
#   c = Inner(N3, a)      l=N3
#   d = Inner(N4, b)      l=N4
#   e = Inner(N5, c)      l=N5
#   f = Inner(d, e)       l=Inner
#
# Filed as ifa/issues/030-polymorphic-dispatch-fat-pointers.md.

class Node:
    def val(self): return 0

class N1(Node):
    def val(self): return 1
class N2(Node):
    def val(self): return 2
class N3(Node):
    def val(self): return 3
class N4(Node):
    def val(self): return 4
class N5(Node):
    def val(self): return 5

class Inner(Node):
    def __init__(self, l, r):
        self.l = l
        self.r = r
    def val(self):
        return self.l.val() + self.r.val()

a = Inner(N1(), N2())   # l=N1, r=N2  → 1+2=3
b = Inner(N2(), N1())   # l=N2, r=N1  → 2+1=3
c = Inner(N3(), a)      # l=N3, r=Inner → 3+3=6
d = Inner(N4(), b)      # l=N4, r=Inner → 4+3=7
e = Inner(N5(), c)      # l=N5, r=Inner → 5+6=11
f = Inner(d, e)         # l=Inner, r=Inner → 7+11=18

print(a.val())   # 3
print(b.val())   # 3
print(c.val())   # 6
print(d.val())   # 7
print(e.val())   # 11
print(f.val())   # 18
