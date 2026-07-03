# Polymorphic dispatch: 2 concrete types at the call site.
#
# Branch.val() calls self.left.val() where left: Leaf | Branch (2 types).
# FA can't monomorphise this because the tree is recursive.
# Expected codegen: conditional-tree (if/else on method pointer).
#
# Filed as ifa/issues/030-polymorphic-dispatch-fat-pointers.md.

class Node:
    def val(self): return 0

class Leaf(Node):
    def __init__(self, v): self.v = v
    def val(self): return self.v

class Branch(Node):
    def __init__(self, left, right):
        self.left = left
        self.right = right
    def val(self):
        return self.left.val() + self.right.val()

t1 = Branch(Leaf(1), Leaf(2))
print(t1.val())                                        # 3

t2 = Branch(Branch(Leaf(3), Leaf(4)), Leaf(5))
print(t2.val())                                        # 12

t3 = Branch(Leaf(10), Branch(Leaf(20), Leaf(30)))
print(t3.val())                                        # 60
