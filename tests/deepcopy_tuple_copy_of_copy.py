# issues/112: deep-copying a tuple TWICE (copy-of-copy) leaves `self[k]`
# unresolved inside tuple.__deepcopy__.
#
# Only manifests once PYC_MAKESEQ / PYC_TUPLE_AS_LIST default on AND
# `class tuple` has a __deepcopy__ -- i.e. it is what blocks the flip.
# Passes at the current defaults, where tuple(iterable) still returns a
# list and lists deep-copy correctly. Replacing `tuple([leaf])` with
# `[leaf]` passes under the flip too, so it is tuple-specific.
#
# Not tagged: it passes as shipped. It is here to pin the shape.
import copy
class T:
    def __init__(self, args=None):
        self.args = args
        self.value = -1
def count(node):
    n = 1
    if node.args:
        for k in range(len(node.args)):
            n += count(node.args[k])
    return n
leaf = T(None)
tree = T(tuple([leaf]))
print(count(tree))
c1 = copy.deepcopy(tree)
c2 = copy.deepcopy(c1)
c2.args[0].value = 77
print(tree.args[0].value, c1.args[0].value, c2.args[0].value)
