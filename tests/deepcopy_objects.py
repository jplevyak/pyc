# issues/029: compiler-synthesized per-class __deepcopy__. Every
# record class without its own __deepcopy__ gets one generated at
# class-build time (gen_class_pyda): shallow clone + per-field
# member.__deepcopy__() dispatch. Lists recurse per element
# (list.__deepcopy__), None is identity, scalars fall back to the
# any-type shallow copy. Exercises: nested objects, Optional[list-of-
# same-class] fields (the genetic2 TreeNode shape), mutation
# isolation, and copy-of-copy.
import copy

class Node:
    def __init__(self, v):
        self.v = v
        self.kids = []

root = Node(1)
a = Node(2)
a.kids.append(Node(4))
root.kids.append(a)
root.kids.append(Node(3))

dup = copy.deepcopy(root)
dup.kids[0].kids[0].v = 99
dup.v = 10
print(root.v)
print(root.kids[0].kids[0].v)
print(dup.v)
print(dup.kids[0].kids[0].v)
print(root.kids[1].v)

# Optional[list-of-same-class] recursive tree (leaves' args is None).
class T:
    def __init__(self, opcode=0, value=-1, args=None):
        self.opcode = opcode
        self.value = value
        self.args = args

def count(node):
    n = 1
    if node.args:
        for k in range(len(node.args)):
            n += count(node.args[k])
    return n

leaf1 = T(0, 5, None)
leaf2 = T(0, 6, None)
tree = T(1, 0, tuple([leaf1, leaf2]))
print(count(tree))
c1 = copy.deepcopy(tree)
print(count(c1))
c2 = copy.deepcopy(c1)   # copy-of-copy: union of original+copy CSs
print(count(c2))
c2.args[0].value = 77
print(tree.args[0].value)
print(c1.args[0].value)
print(c2.args[0].value)
